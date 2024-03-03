#pragma once

#include <cstddef>
#include <string>
#include <Core/Defines.h>
#include <Interpreters/Cache/FileCache_fwd.h>
#include <Common/Throttler_fwd.h>
#include <Common/Priority.h>
#include <Common/Scheduler/ResourceLink.h>

namespace DB
{
enum class LocalFSReadMethod
{
    /**
     * Simple synchronous reads with 'read'.
     * Can use direct IO after specified size.
     * Can use prefetch by asking OS to perform readahead.
     */
    read,

    /**
     * Simple synchronous reads with 'pread'.
     * In contrast to 'read', shares single file descriptor from multiple threads.
     * Can use direct IO after specified size.
     * Can use prefetch by asking OS to perform readahead.
     */
    pread,

    /**
     * Use mmap after specified size or simple synchronous reads with 'pread'.
     * Can use prefetch by asking OS to perform readahead.
     */
    mmap,

    /**
     * Use the io_uring Linux subsystem for asynchronous reads.
     * Can use direct IO after specified size.
     * Can do prefetch with double buffering.
     */
    io_uring,

    /**
     * Checks if data is in page cache with 'preadv2' on modern Linux kernels.
     * If data is in page cache, read from the same thread.
     * If not, offload IO to separate threadpool.
     * Can do prefetch with double buffering.
     * Can use specified priorities and limit the number of concurrent reads.
     */
    pread_threadpool,

    /// Use asynchronous reader with fake backend that in fact synchronous.
    /// @attention Use only for testing purposes.
    pread_fake_async
};

enum class RemoteFSReadMethod
{
    read,
    threadpool,
};

class MMappedFileCache;
class PageCache;

struct ReadSettings
{
    /// Method to use reading from local filesystem.
    LocalFSReadMethod local_fs_method = LocalFSReadMethod::pread;
    /// Method to use reading from remote filesystem.
    RemoteFSReadMethod remote_fs_method = RemoteFSReadMethod::threadpool;

    /// https://eklitzke.org/efficient-file-copying-on-linux
    size_t local_fs_buffer_size = 128 * 1024;

    size_t remote_fs_buffer_size = DBMS_DEFAULT_BUFFER_SIZE;
    size_t prefetch_buffer_size = DBMS_DEFAULT_BUFFER_SIZE;

    bool local_fs_prefetch = false;
    bool remote_fs_prefetch = false;

    /// For 'read', 'pread' and 'pread_threadpool' methods.
    size_t direct_io_threshold = 0;

    /// For 'mmap' method.
    size_t mmap_threshold = 0;
    MMappedFileCache * mmap_cache = nullptr;

    /// For 'pread_threadpool'/'io_uring' method. Lower value is higher priority.
    Priority priority;

    bool load_marks_asynchronously = true;

    size_t remote_fs_read_max_backoff_ms = 10000;
    size_t remote_fs_read_backoff_max_tries = 4;

    bool enable_filesystem_read_prefetches_log = false;

    bool enable_filesystem_cache = true;
    bool read_from_filesystem_cache_if_exists_otherwise_bypass_cache = false;
    bool enable_filesystem_cache_log = false;
    /// Don't populate cache when the read is not part of query execution (e.g. background thread).
    bool avoid_readthrough_cache_outside_query_context = true;
    size_t filesystem_cache_segments_batch_size = 20;

    bool use_page_cache_for_disks_without_file_cache = false;
    bool read_from_page_cache_if_exists_otherwise_bypass_cache = false;
    bool page_cache_inject_eviction = false;
    std::shared_ptr<PageCache> page_cache;

    size_t filesystem_cache_max_download_size = (128UL * 1024 * 1024 * 1024);
    bool skip_download_if_exceeds_query_cache = true;

    size_t remote_read_min_bytes_for_seek = DBMS_DEFAULT_BUFFER_SIZE;

    FileCachePtr remote_fs_cache;

    /// Bandwidth throttler to use during reading
    ThrottlerPtr remote_throttler;
    ThrottlerPtr local_throttler;

    // Resource to be used during reading
    ResourceLink resource_link;

    size_t http_max_tries = 10;
    size_t http_retry_initial_backoff_ms = 100;
    size_t http_retry_max_backoff_ms = 1600;
    bool http_skip_not_found_url_for_globs = true;
    bool http_make_head_request = true;

    /// Monitoring
    bool for_object_storage = false; // to choose which profile events should be incremented

    ReadSettings adjustBufferSize(size_t file_size) const
    {
        ReadSettings res = *this;
        res.local_fs_buffer_size = std::min(std::max(1ul, file_size), local_fs_buffer_size);
        res.remote_fs_buffer_size = std::min(std::max(1ul, file_size), remote_fs_buffer_size);
        res.prefetch_buffer_size = std::min(std::max(1ul, file_size), prefetch_buffer_size);
        return res;
    }
};

}
