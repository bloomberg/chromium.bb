// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/single_thread_task_runner.h"
#include "base/task_scheduler/task_scheduler.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_cleanup_tracker.h"
#include "net/disk_cache/blockfile/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/disk_cache.h"
#include "net/disk_cache/memory/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

namespace {

// Builds an instance of the backend depending on platform, type, experiments
// etc. Takes care of the retry state. This object will self-destroy when
// finished.
class CacheCreator {
 public:
  CacheCreator(const base::FilePath& path,
               bool force,
               int max_bytes,
               net::CacheType type,
               net::BackendType backend_type,
               uint32_t flags,
               net::NetLog* net_log,
               std::unique_ptr<disk_cache::Backend>* backend,
               base::OnceClosure post_cleanup_callback,
               const net::CompletionCallback& callback);

  int TryCreateCleanupTrackerAndRun();

  // Creates the backend, the cleanup context for it having been already
  // established... or purposefully left as null.
  int Run();

 private:
  ~CacheCreator();

  void DoCallback(int result);

  void OnIOComplete(int result);

  const base::FilePath path_;
  bool force_;
  bool retry_;
  int max_bytes_;
  net::CacheType type_;
  net::BackendType backend_type_;
#if !defined(OS_ANDROID)
  uint32_t flags_;
#endif
  std::unique_ptr<disk_cache::Backend>* backend_;
  base::OnceClosure post_cleanup_callback_;
  net::CompletionCallback callback_;
  std::unique_ptr<disk_cache::Backend> created_cache_;
  net::NetLog* net_log_;
  scoped_refptr<disk_cache::BackendCleanupTracker> cleanup_tracker_;

  DISALLOW_COPY_AND_ASSIGN(CacheCreator);
};

CacheCreator::CacheCreator(
    const base::FilePath& path,
    bool force,
    int max_bytes,
    net::CacheType type,
    net::BackendType backend_type,
    uint32_t flags,
    net::NetLog* net_log,
    std::unique_ptr<disk_cache::Backend>* backend,
    base::OnceClosure post_cleanup_callback,
    const net::CompletionCallback& callback)
    : path_(path),
      force_(force),
      retry_(false),
      max_bytes_(max_bytes),
      type_(type),
      backend_type_(backend_type),
#if !defined(OS_ANDROID)
      flags_(flags),
#endif
      backend_(backend),
      post_cleanup_callback_(std::move(post_cleanup_callback)),
      callback_(callback),
      net_log_(net_log) {
}

CacheCreator::~CacheCreator() {}

int CacheCreator::Run() {
#if defined(OS_ANDROID)
  static const bool kSimpleBackendIsDefault = true;
#else
  static const bool kSimpleBackendIsDefault = false;
#endif
  if (backend_type_ == net::CACHE_BACKEND_SIMPLE ||
      (backend_type_ == net::CACHE_BACKEND_DEFAULT &&
       kSimpleBackendIsDefault)) {
    disk_cache::SimpleBackendImpl* simple_cache =
        new disk_cache::SimpleBackendImpl(
            path_, cleanup_tracker_.get(), max_bytes_, type_,
            /* cache_thread = */ nullptr, net_log_);
    created_cache_.reset(simple_cache);
    return simple_cache->Init(
        base::Bind(&CacheCreator::OnIOComplete, base::Unretained(this)));
  }

// Avoid references to blockfile functions on Android to reduce binary size.
#if defined(OS_ANDROID)
  return net::ERR_FAILED;
#else
  disk_cache::BackendImpl* new_cache = new disk_cache::BackendImpl(
      path_, cleanup_tracker_.get(), /*cache_thread = */ nullptr, net_log_);
  created_cache_.reset(new_cache);
  new_cache->SetMaxSize(max_bytes_);
  new_cache->SetType(type_);
  new_cache->SetFlags(flags_);
  int rv = new_cache->Init(
      base::Bind(&CacheCreator::OnIOComplete, base::Unretained(this)));
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
  return rv;
#endif
}

int CacheCreator::TryCreateCleanupTrackerAndRun() {
  // Before creating a cache Backend, a BackendCleanupTracker object is needed
  // so there is a place to keep track of outstanding I/O even after the backend
  // object itself is destroyed, so that further use of the directory
  // doesn't race with those outstanding disk I/O ops.

  // This method's purpose it to grab exlusive ownership of a fresh
  // BackendCleanupTracker for the cache path, and then move on to Run(),
  // which will take care of creating the actual cache backend. It's possible
  // that something else is currently making use of the directory, in which
  // case BackendCleanupTracker::TryCreate will fail, but will just have
  // TryCreateCleanupTrackerAndRun run again at an opportune time to make
  // another attempt.

  // The resulting BackendCleanupTracker is stored into a scoped_refptr member
  // so that it's kept alive while |this| CacheCreator exists , so that in the
  // case Run() needs to retry Backend creation the same BackendCleanupTracker
  // is used for both attempts, and |post_cleanup_callback_| gets called after
  // the second try, not the first one.
  cleanup_tracker_ = disk_cache::BackendCleanupTracker::TryCreate(
      path_, base::BindOnce(base::IgnoreResult(
                                &CacheCreator::TryCreateCleanupTrackerAndRun),
                            base::Unretained(this)));
  if (!cleanup_tracker_)
    return net::ERR_IO_PENDING;
  if (!post_cleanup_callback_.is_null())
    cleanup_tracker_->AddPostCleanupCallback(std::move(post_cleanup_callback_));
  return Run();
}

void CacheCreator::DoCallback(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result == net::OK) {
    *backend_ = std::move(created_cache_);
  } else {
    LOG(ERROR) << "Unable to create cache";
    created_cache_.reset();
  }
  callback_.Run(result);
  delete this;
}

// If the initialization of the cache fails, and |force| is true, we will
// discard the whole cache and create a new one.
void CacheCreator::OnIOComplete(int result) {
  if (result == net::OK || !force_ || retry_)
    return DoCallback(result);

  // This is a failure and we are supposed to try again, so delete the object,
  // delete all the files, and try again.
  retry_ = true;
  created_cache_.reset();
  if (!disk_cache::DelayedCacheCleanup(path_))
    return DoCallback(result);

  // The worker thread will start deleting files soon, but the original folder
  // is not there anymore... let's create a new set of files.
  int rv = Run();
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

}  // namespace

namespace disk_cache {

int CreateCacheBackendImpl(
    net::CacheType type,
    net::BackendType backend_type,
    const base::FilePath& path,
    int max_bytes,
    bool force,
    net::NetLog* net_log,
    std::unique_ptr<Backend>* backend,
    base::OnceClosure post_cleanup_callback,
    const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());

  if (type == net::MEMORY_CACHE) {
    std::unique_ptr<MemBackendImpl> mem_backend_impl =
        disk_cache::MemBackendImpl::CreateBackend(max_bytes, net_log);
    if (mem_backend_impl) {
      mem_backend_impl->SetPostCleanupCallback(
          std::move(post_cleanup_callback));
      *backend = std::move(mem_backend_impl);
      return net::OK;
    } else {
      if (!post_cleanup_callback.is_null())
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, std::move(post_cleanup_callback));
      return net::ERR_FAILED;
    }
  }

  bool had_post_cleanup_callback = !post_cleanup_callback.is_null();
  CacheCreator* creator = new CacheCreator(
      path, force, max_bytes, type, backend_type, kNone, net_log, backend,
      std::move(post_cleanup_callback), callback);
  if (type == net::DISK_CACHE || type == net::MEDIA_CACHE) {
    DCHECK(!had_post_cleanup_callback);
    return creator->Run();
  }

  return creator->TryCreateCleanupTrackerAndRun();
}

int CreateCacheBackend(net::CacheType type,
                       net::BackendType backend_type,
                       const base::FilePath& path,
                       int max_bytes,
                       bool force,
                       net::NetLog* net_log,
                       std::unique_ptr<Backend>* backend,
                       const net::CompletionCallback& callback) {
  return CreateCacheBackendImpl(type, backend_type, path, max_bytes, force,
                                net_log, backend, base::OnceClosure(),
                                callback);
}

int CreateCacheBackend(net::CacheType type,
                       net::BackendType backend_type,
                       const base::FilePath& path,
                       int max_bytes,
                       bool force,
                       net::NetLog* net_log,
                       std::unique_ptr<Backend>* backend,
                       base::OnceClosure post_cleanup_callback,
                       const net::CompletionCallback& callback) {
  return CreateCacheBackendImpl(type, backend_type, path, max_bytes, force,
                                net_log, backend,
                                std::move(post_cleanup_callback), callback);
}

void FlushCacheThreadForTesting() {
  // For simple backend.
  SimpleBackendImpl::FlushWorkerPoolForTesting();
  base::TaskScheduler::GetInstance()->FlushForTesting();

  // Block backend.
  BackendImpl::FlushForTesting();
}

int Backend::CalculateSizeOfEntriesBetween(base::Time initial_time,
                                           base::Time end_time,
                                           const CompletionCallback& callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

uint8_t Backend::GetEntryInMemoryData(const std::string& key) {
  return 0;
}

void Backend::SetEntryInMemoryData(const std::string& key, uint8_t data) {}

}  // namespace disk_cache
