// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/metrics/field_trial.h"
#include "base/stringprintf.h"
#include "net/base/net_errors.h"
#include "net/disk_cache/backend_impl.h"
#include "net/disk_cache/cache_util.h"
#include "net/disk_cache/mem_backend_impl.h"
#include "net/disk_cache/simple/simple_backend_impl.h"

#ifdef USE_TRACING_CACHE_BACKEND
#include "net/disk_cache/tracing_cache_backend.h"
#endif

namespace {

// Builds an instance of the backend depending on platform, type, experiments
// etc. Takes care of the retry state. This object will self-destroy when
// finished.
class CacheCreator {
 public:
  CacheCreator(const base::FilePath& path, bool force, int max_bytes,
               net::CacheType type, uint32 flags,
               base::MessageLoopProxy* thread, net::NetLog* net_log,
               disk_cache::Backend** backend,
               const net::CompletionCallback& callback);

  // Creates the backend.
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
  uint32 flags_;
  scoped_refptr<base::MessageLoopProxy> thread_;
  disk_cache::Backend** backend_;
  net::CompletionCallback callback_;
  disk_cache::Backend* created_cache_;
  net::NetLog* net_log_;

  DISALLOW_COPY_AND_ASSIGN(CacheCreator);
};

CacheCreator::CacheCreator(
    const base::FilePath& path, bool force, int max_bytes,
    net::CacheType type, uint32 flags,
    base::MessageLoopProxy* thread, net::NetLog* net_log,
    disk_cache::Backend** backend,
    const net::CompletionCallback& callback)
    : path_(path),
      force_(force),
      retry_(false),
      max_bytes_(max_bytes),
      type_(type),
      flags_(flags),
      thread_(thread),
      backend_(backend),
      callback_(callback),
      created_cache_(NULL),
      net_log_(net_log) {
  }

CacheCreator::~CacheCreator() {
}

int CacheCreator::Run() {
  // TODO(pasko): The two caches should never coexist on disk. Each individual
  // cache backend should fail to initialize if it observes the index that does
  // not belong to it.
  const std::string experiment_name =
      base::FieldTrialList::FindFullName("SimpleCacheTrial");
  if (experiment_name == "ExplicitYes" || experiment_name == "ExperimentYes" ||
      experiment_name == "ExperimentYes2") {
    // TODO(gavinp,pasko): While simple backend development proceeds, we're only
    // testing it against net::DISK_CACHE. Turn it on for more cache types as
    // appropriate.
    if (type_ == net::DISK_CACHE) {
      disk_cache::SimpleBackendImpl* simple_cache =
          new disk_cache::SimpleBackendImpl(path_, max_bytes_, type_, thread_,
                                            net_log_);
      created_cache_ = simple_cache;
      return simple_cache->Init(
          base::Bind(&CacheCreator::OnIOComplete, base::Unretained(this)));
    }
  }
  disk_cache::BackendImpl* new_cache =
      new disk_cache::BackendImpl(path_, thread_, net_log_);
  created_cache_ = new_cache;
  new_cache->SetMaxSize(max_bytes_);
  new_cache->SetType(type_);
  new_cache->SetFlags(flags_);
  int rv = new_cache->Init(
      base::Bind(&CacheCreator::OnIOComplete, base::Unretained(this)));
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
  return rv;
}

void CacheCreator::DoCallback(int result) {
  DCHECK_NE(net::ERR_IO_PENDING, result);
  if (result == net::OK) {
#ifndef USE_TRACING_CACHE_BACKEND
    *backend_ = created_cache_;
#else
    *backend_ = new disk_cache::TracingCacheBackend(created_cache_);
#endif
  } else {
    LOG(ERROR) << "Unable to create cache";
    *backend_ = NULL;
    delete created_cache_;
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
  delete created_cache_;
  created_cache_ = NULL;
  if (!disk_cache::DelayedCacheCleanup(path_))
    return DoCallback(result);

  // The worker thread will start deleting files soon, but the original folder
  // is not there anymore... let's create a new set of files.
  int rv = Run();
  DCHECK_EQ(net::ERR_IO_PENDING, rv);
}

}  // namespace

namespace disk_cache {

int CreateCacheBackend(net::CacheType type, const base::FilePath& path,
                       int max_bytes,
                       bool force, base::MessageLoopProxy* thread,
                       net::NetLog* net_log, Backend** backend,
                       const net::CompletionCallback& callback) {
  DCHECK(!callback.is_null());
  if (type == net::MEMORY_CACHE) {
    *backend = disk_cache::MemBackendImpl::CreateBackend(max_bytes, net_log);
    return *backend ? net::OK : net::ERR_FAILED;
  }
  DCHECK(thread);
  CacheCreator* creator = new CacheCreator(path, force, max_bytes, type, kNone,
                                           thread, net_log, backend, callback);
  return creator->Run();
}

}  // namespace disk_cache
