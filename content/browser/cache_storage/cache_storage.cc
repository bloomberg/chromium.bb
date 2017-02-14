// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage.h"

#include <stddef.h>

#include <set>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/guid.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_cache_handle.h"
#include "content/browser/cache_storage/cache_storage_index.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

std::string HexedHash(const std::string& value) {
  std::string value_hash = base::SHA1HashString(value);
  std::string valued_hexed_hash = base::ToLowerASCII(
      base::HexEncode(value_hash.c_str(), value_hash.length()));
  return valued_hexed_hash;
}

void SizeRetrievedFromAllCaches(std::unique_ptr<int64_t> accumulator,
                                const CacheStorage::SizeCallback& callback) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(callback, *accumulator));
}

void DoNothingWithBool(bool success) {}

}  // namespace

const char CacheStorage::kIndexFileName[] = "index.txt";
constexpr int64_t CacheStorage::kSizeUnknown;

struct CacheStorage::CacheMatchResponse {
  CacheMatchResponse() = default;
  ~CacheMatchResponse() = default;

  CacheStorageError error;
  std::unique_ptr<ServiceWorkerResponse> service_worker_response;
  std::unique_ptr<storage::BlobDataHandle> blob_data_handle;
};

// Handles the loading and clean up of CacheStorageCache objects.
class CacheStorage::CacheLoader {
 public:
  typedef base::Callback<void(std::unique_ptr<CacheStorageCache>)>
      CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  using CacheStorageIndexLoadCallback =
      base::Callback<void(std::unique_ptr<CacheStorageIndex>)>;

  CacheLoader(
      base::SequencedTaskRunner* cache_task_runner,
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      storage::QuotaManagerProxy* quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      CacheStorage* cache_storage,
      const GURL& origin)
      : cache_task_runner_(cache_task_runner),
        request_context_getter_(request_context_getter),
        quota_manager_proxy_(quota_manager_proxy),
        blob_context_(blob_context),
        cache_storage_(cache_storage),
        origin_(origin) {
    DCHECK(!origin_.is_empty());
  }

  virtual ~CacheLoader() {}

  // Creates a CacheStorageCache with the given name. It does not attempt to
  // load the backend, that happens lazily when the cache is used.
  virtual std::unique_ptr<CacheStorageCache> CreateCache(
      const std::string& cache_name,
      int64_t cache_size) = 0;

  // Deletes any pre-existing cache of the same name and then loads it.
  virtual void PrepareNewCacheDestination(const std::string& cache_name,
                                          const CacheCallback& callback) = 0;

  // After the backend has been deleted, do any extra house keeping such as
  // removing the cache's directory.
  virtual void CleanUpDeletedCache(CacheStorageCache* cache) = 0;

  // Writes the cache index to disk if applicable.
  virtual void WriteIndex(const CacheStorageIndex& index,
                          const BoolCallback& callback) = 0;

  // Loads the cache index from disk if applicable.
  virtual void LoadIndex(const CacheStorageIndexLoadCallback& callback) = 0;

  // Called when CacheStorage has created a cache. Used to hold onto a handle to
  // the cache if necessary.
  virtual void NotifyCacheCreated(
      const std::string& cache_name,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) {}

  // Notification that the cache for |cache_handle| has been doomed. If the
  // loader is holding a handle to the cache, it should drop it now.
  virtual void NotifyCacheDoomed(
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) {}

 protected:
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;

  // Owned by CacheStorage which owns this.
  storage::QuotaManagerProxy* quota_manager_proxy_;

  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  // Raw pointer is safe because this object is owned by cache_storage_.
  CacheStorage* cache_storage_;

  GURL origin_;
};

// Creates memory-only ServiceWorkerCaches. Because these caches have no
// persistent storage it is not safe to free them from memory if they might be
// used again. Therefore this class holds a reference to each cache until the
// cache is doomed.
class CacheStorage::MemoryLoader : public CacheStorage::CacheLoader {
 public:
  MemoryLoader(base::SequencedTaskRunner* cache_task_runner,
               scoped_refptr<net::URLRequestContextGetter> request_context,
               storage::QuotaManagerProxy* quota_manager_proxy,
               base::WeakPtr<storage::BlobStorageContext> blob_context,
               CacheStorage* cache_storage,
               const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    cache_storage,
                    origin) {}

  std::unique_ptr<CacheStorageCache> CreateCache(const std::string& cache_name,
                                                 int64_t cache_size) override {
    return CacheStorageCache::CreateMemoryCache(
        origin_, cache_name, cache_storage_, request_context_getter_,
        quota_manager_proxy_, blob_context_);
  }

  void PrepareNewCacheDestination(const std::string& cache_name,
                                  const CacheCallback& callback) override {
    std::unique_ptr<CacheStorageCache> cache =
        CreateCache(cache_name, 0 /*cache_size*/);
    callback.Run(std::move(cache));
  }

  void CleanUpDeletedCache(CacheStorageCache* cache) override {}

  void WriteIndex(const CacheStorageIndex& index,
                  const BoolCallback& callback) override {
    callback.Run(true);
  }

  void LoadIndex(const CacheStorageIndexLoadCallback& callback) override {
    callback.Run(base::MakeUnique<CacheStorageIndex>());
  }

  void NotifyCacheCreated(
      const std::string& cache_name,
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) override {
    DCHECK(!base::ContainsKey(cache_handles_, cache_name));
    cache_handles_.insert(std::make_pair(cache_name, std::move(cache_handle)));
  };

  void NotifyCacheDoomed(
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) override {
    DCHECK(
        base::ContainsKey(cache_handles_, cache_handle->value()->cache_name()));
    cache_handles_.erase(cache_handle->value()->cache_name());
  };

 private:
  typedef std::map<std::string, std::unique_ptr<CacheStorageCacheHandle>>
      CacheHandles;
  ~MemoryLoader() override {}

  // Keep a reference to each cache to ensure that it's not freed before the
  // client calls CacheStorage::Delete or the CacheStorage is
  // freed.
  CacheHandles cache_handles_;
};

class CacheStorage::SimpleCacheLoader : public CacheStorage::CacheLoader {
 public:
  SimpleCacheLoader(const base::FilePath& origin_path,
                    base::SequencedTaskRunner* cache_task_runner,
                    scoped_refptr<net::URLRequestContextGetter> request_context,
                    storage::QuotaManagerProxy* quota_manager_proxy,
                    base::WeakPtr<storage::BlobStorageContext> blob_context,
                    CacheStorage* cache_storage,
                    const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    cache_storage,
                    origin),
        origin_path_(origin_path),
        weak_ptr_factory_(this) {}

  std::unique_ptr<CacheStorageCache> CreateCache(const std::string& cache_name,
                                                 int64_t cache_size) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(base::ContainsKey(cache_name_to_cache_dir_, cache_name));

    std::string cache_dir = cache_name_to_cache_dir_[cache_name];
    base::FilePath cache_path = origin_path_.AppendASCII(cache_dir);
    return CacheStorageCache::CreatePersistentCache(
        origin_, cache_name, cache_storage_, cache_path,
        request_context_getter_, quota_manager_proxy_, blob_context_,
        cache_size);
  }

  void PrepareNewCacheDestination(const std::string& cache_name,
                                  const CacheCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::PrepareNewCacheDirectoryInPool,
                   origin_path_),
        base::Bind(&SimpleCacheLoader::PrepareNewCacheCreateCache,
                   weak_ptr_factory_.GetWeakPtr(), cache_name, callback));
  }

  // Runs on the cache_task_runner_.
  static std::string PrepareNewCacheDirectoryInPool(
      const base::FilePath& origin_path) {
    std::string cache_dir;
    base::FilePath cache_path;
    do {
      cache_dir = base::GenerateGUID();
      cache_path = origin_path.AppendASCII(cache_dir);
    } while (base::PathExists(cache_path));

    return base::CreateDirectory(cache_path) ? cache_dir : "";
  }

  void PrepareNewCacheCreateCache(const std::string& cache_name,
                                  const CacheCallback& callback,
                                  const std::string& cache_dir) {
    if (cache_dir.empty()) {
      callback.Run(std::unique_ptr<CacheStorageCache>());
      return;
    }

    cache_name_to_cache_dir_[cache_name] = cache_dir;
    callback.Run(CreateCache(cache_name, CacheStorage::kSizeUnknown));
  }

  void CleanUpDeletedCache(CacheStorageCache* cache) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(base::ContainsKey(doomed_cache_to_path_, cache));

    base::FilePath cache_path =
        origin_path_.AppendASCII(doomed_cache_to_path_[cache]);
    doomed_cache_to_path_.erase(cache);

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&SimpleCacheLoader::CleanUpDeleteCacheDirInPool,
                              cache_path));
  }

  static void CleanUpDeleteCacheDirInPool(const base::FilePath& cache_path) {
    base::DeleteFile(cache_path, true /* recursive */);
  }

  void WriteIndex(const CacheStorageIndex& index,
                  const BoolCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    proto::CacheStorageIndex protobuf_index;
    protobuf_index.set_origin(origin_.spec());

    for (const auto& cache_metadata : index.ordered_cache_metadata()) {
      DCHECK(base::ContainsKey(cache_name_to_cache_dir_, cache_metadata.name));

      proto::CacheStorageIndex::Cache* index_cache = protobuf_index.add_cache();
      index_cache->set_name(cache_metadata.name);
      index_cache->set_cache_dir(cache_name_to_cache_dir_[cache_metadata.name]);
      if (cache_metadata.size == CacheStorage::kSizeUnknown)
        index_cache->clear_size();
      else
        index_cache->set_size(cache_metadata.size);
    }

    std::string serialized;
    bool success = protobuf_index.SerializeToString(&serialized);
    DCHECK(success);

    base::FilePath tmp_path = origin_path_.AppendASCII("index.txt.tmp");
    base::FilePath index_path =
        origin_path_.AppendASCII(CacheStorage::kIndexFileName);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::WriteIndexWriteToFileInPool, tmp_path,
                   index_path, serialized),
        callback);
  }

  static bool WriteIndexWriteToFileInPool(const base::FilePath& tmp_path,
                                          const base::FilePath& index_path,
                                          const std::string& data) {
    int bytes_written = base::WriteFile(tmp_path, data.c_str(), data.size());
    if (bytes_written != base::checked_cast<int>(data.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      return false;
    }

    // Atomically rename the temporary index file to become the real one.
    return base::ReplaceFile(tmp_path, index_path, NULL);
  }

  void LoadIndex(const CacheStorageIndexLoadCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::ReadAndMigrateIndexInPool, origin_path_),
        base::Bind(&SimpleCacheLoader::LoadIndexDidReadIndex,
                   weak_ptr_factory_.GetWeakPtr(), callback));
  }

  void LoadIndexDidReadIndex(const CacheStorageIndexLoadCallback& callback,
                             proto::CacheStorageIndex protobuf_index) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    std::unique_ptr<std::set<std::string>> cache_dirs(
        new std::set<std::string>);

    auto index = base::MakeUnique<CacheStorageIndex>();
    for (int i = 0, max = protobuf_index.cache_size(); i < max; ++i) {
      const proto::CacheStorageIndex::Cache& cache = protobuf_index.cache(i);
      DCHECK(cache.has_cache_dir());
      int64_t cache_size =
          cache.has_size() ? cache.size() : CacheStorage::kSizeUnknown;
      index->Insert(CacheStorageIndex::CacheMetadata(cache.name(), cache_size));
      cache_name_to_cache_dir_[cache.name()] = cache.cache_dir();
      cache_dirs->insert(cache.cache_dir());
    }

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteUnreferencedCachesInPool, origin_path_,
                              base::Passed(&cache_dirs)));
    callback.Run(std::move(index));
  }

  void NotifyCacheDoomed(
      std::unique_ptr<CacheStorageCacheHandle> cache_handle) override {
    DCHECK(base::ContainsKey(cache_name_to_cache_dir_,
                             cache_handle->value()->cache_name()));
    auto iter =
        cache_name_to_cache_dir_.find(cache_handle->value()->cache_name());
    doomed_cache_to_path_[cache_handle->value()] = iter->second;
    cache_name_to_cache_dir_.erase(iter);
  };

 private:
  friend class MigratedLegacyCacheDirectoryNameTest;
  ~SimpleCacheLoader() override {}

  // Iterates over the caches and deletes any directory not found in
  // |cache_dirs|. Runs on cache_task_runner_
  static void DeleteUnreferencedCachesInPool(
      const base::FilePath& cache_base_dir,
      std::unique_ptr<std::set<std::string>> cache_dirs) {
    base::FileEnumerator file_enum(cache_base_dir, false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);
    std::vector<base::FilePath> dirs_to_delete;
    base::FilePath cache_path;
    while (!(cache_path = file_enum.Next()).empty()) {
      if (!base::ContainsKey(*cache_dirs, cache_path.BaseName().AsUTF8Unsafe()))
        dirs_to_delete.push_back(cache_path);
    }

    for (const base::FilePath& cache_path : dirs_to_delete)
      base::DeleteFile(cache_path, true /* recursive */);
  }

  // Runs on cache_task_runner_
  static proto::CacheStorageIndex ReadAndMigrateIndexInPool(
      const base::FilePath& origin_path) {
    const base::FilePath index_path =
        origin_path.AppendASCII(CacheStorage::kIndexFileName);

    proto::CacheStorageIndex index;
    std::string body;
    if (!base::ReadFileToString(index_path, &body) ||
        !index.ParseFromString(body))
      return proto::CacheStorageIndex();
    body.clear();

    base::File::Info file_info;
    base::Time index_last_modified;
    if (GetFileInfo(index_path, &file_info))
      index_last_modified = file_info.last_modified;
    bool index_modified = false;

    // Look for caches that have no cache_dir. Give any such caches a directory
    // with a random name and move them there. Then, rewrite the index file.
    // Additionally invalidate the size of any index entries where the cache was
    // modified after the index (making it out-of-date).
    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const proto::CacheStorageIndex::Cache& cache = index.cache(i);
      if (cache.has_cache_dir()) {
        if (cache.has_size()) {
          base::FilePath cache_dir = origin_path.AppendASCII(cache.cache_dir());
          if (!GetFileInfo(cache_dir, &file_info) ||
              index_last_modified <= file_info.last_modified) {
            // Index is older than this cache, so invalidate index entries that
            // may change as a result of cache operations.
            index.mutable_cache(i)->clear_size();
          }
        }
      } else {
        // Find a new home for the cache.
        base::FilePath legacy_cache_path =
            origin_path.AppendASCII(HexedHash(cache.name()));
        std::string cache_dir;
        base::FilePath cache_path;
        do {
          cache_dir = base::GenerateGUID();
          cache_path = origin_path.AppendASCII(cache_dir);
        } while (base::PathExists(cache_path));

        if (!base::Move(legacy_cache_path, cache_path)) {
          // If the move fails then the cache is in a bad state. Return an empty
          // index so that the CacheStorage can start fresh. The unreferenced
          // caches will be discarded later in initialization.
          return proto::CacheStorageIndex();
        }

        index.mutable_cache(i)->set_cache_dir(cache_dir);
        index.mutable_cache(i)->clear_size();
        index_modified = true;
      }
    }

    if (index_modified) {
      base::FilePath tmp_path = origin_path.AppendASCII("index.txt.tmp");
      if (!index.SerializeToString(&body) ||
          !WriteIndexWriteToFileInPool(tmp_path, index_path, body)) {
        return proto::CacheStorageIndex();
      }
    }

    return index;
  }

  const base::FilePath origin_path_;
  std::map<std::string, std::string> cache_name_to_cache_dir_;
  std::map<CacheStorageCache*, std::string> doomed_cache_to_path_;

  base::WeakPtrFactory<SimpleCacheLoader> weak_ptr_factory_;
};

CacheStorage::CacheStorage(
    const base::FilePath& path,
    bool memory_only,
    base::SequencedTaskRunner* cache_task_runner,
    scoped_refptr<net::URLRequestContextGetter> request_context,
    scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    const GURL& origin)
    : initialized_(false),
      initializing_(false),
      memory_only_(memory_only),
      scheduler_(new CacheStorageScheduler(
          CacheStorageSchedulerClient::CLIENT_STORAGE)),
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      origin_(origin),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader(
        cache_task_runner_.get(), std::move(request_context),
        quota_manager_proxy.get(), blob_context, this, origin));
  else
    cache_loader_.reset(new SimpleCacheLoader(
        origin_path_, cache_task_runner_.get(), std::move(request_context),
        quota_manager_proxy.get(), blob_context, this, origin));
}

CacheStorage::~CacheStorage() {
}

void CacheStorage::OpenCache(const std::string& cache_name,
                             const CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::OpenCacheImpl, weak_factory_.GetWeakPtr(),
                 cache_name, scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::HasCache(const std::string& cache_name,
                            const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::HasCacheImpl, weak_factory_.GetWeakPtr(),
                 cache_name, scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::DeleteCache(const std::string& cache_name,
                               const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::DeleteCacheImpl, weak_factory_.GetWeakPtr(),
                 cache_name, scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::EnumerateCaches(const IndexCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::EnumerateCachesImpl, weak_factory_.GetWeakPtr(),
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::MatchCache(
    const std::string& cache_name,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::MatchCacheImpl, weak_factory_.GetWeakPtr(),
                 cache_name, base::Passed(std::move(request)), match_params,
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::MatchAllCaches(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  quota_manager_proxy_->NotifyStorageAccessed(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary);

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::MatchAllCachesImpl, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), match_params,
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::GetSizeThenCloseAllCaches(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorage::GetSizeThenCloseAllCachesImpl, weak_factory_.GetWeakPtr(),
      scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::Size(const CacheStorage::SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::SizeImpl, weak_factory_.GetWeakPtr(),
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::ScheduleWriteIndex() {
  static const int64_t kWriteIndexDelaySecs = 5;
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  index_write_task_.Reset(base::Bind(&CacheStorage::WriteIndex,
                                     weak_factory_.GetWeakPtr(),
                                     base::Bind(&DoNothingWithBool)));
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, index_write_task_.callback(),
      base::TimeDelta::FromSeconds(kWriteIndexDelaySecs));
}

void CacheStorage::WriteIndex(const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::WriteIndexImpl, weak_factory_.GetWeakPtr(),
                 scheduler_->WrapCallbackToRunNext(callback)));
}

void CacheStorage::WriteIndexImpl(const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  cache_loader_->WriteIndex(*cache_index_, callback);
}

bool CacheStorage::InitiateScheduledIndexWriteForTest(
    const base::Callback<void(bool)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (index_write_pending()) {
    index_write_task_.Cancel();
    WriteIndex(callback);
    return true;
  }
  callback.Run(true /* success */);
  return false;
}

void CacheStorage::CacheSizeUpdated(const CacheStorageCache* cache,
                                    int64_t size) {
  // Should not be called for doomed caches.
  DCHECK(!base::ContainsKey(doomed_caches_,
                            const_cast<CacheStorageCache*>(cache)));
  cache_index_->SetCacheSize(cache->cache_name(), size);
  ScheduleWriteIndex();
}

void CacheStorage::StartAsyncOperationForTesting() {
  scheduler_->ScheduleOperation(base::Bind(&base::DoNothing));
}

void CacheStorage::CompleteAsyncOperationForTesting() {
  scheduler_->CompleteOperationAndRunNext();
}

// Init is run lazily so that it is called on the proper MessageLoop.
void CacheStorage::LazyInit() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);

  if (initializing_)
    return;

  DCHECK(!scheduler_->ScheduledOperations());

  initializing_ = true;
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::LazyInitImpl, weak_factory_.GetWeakPtr()));
}

void CacheStorage::LazyInitImpl() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);
  DCHECK(initializing_);

  // 1. Get the cache index (async call)
  // 2. For each cache name, load the cache (async call)
  // 3. Once each load is complete, update the map variables.
  // 4. Call the list of waiting callbacks.

  cache_loader_->LoadIndex(base::Bind(&CacheStorage::LazyInitDidLoadIndex,
                                      weak_factory_.GetWeakPtr()));
}

void CacheStorage::LazyInitDidLoadIndex(
    std::unique_ptr<CacheStorageIndex> index) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_map_.empty());

  for (const auto& cache_metadata : index->ordered_cache_metadata()) {
    cache_map_.insert(std::make_pair(cache_metadata.name,
                                     std::unique_ptr<CacheStorageCache>()));
  }

  DCHECK(!cache_index_);
  cache_index_ = std::move(index);

  initializing_ = false;
  initialized_ = true;

  scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::OpenCacheImpl(const std::string& cache_name,
                                 const CacheAndErrorCallback& callback) {
  std::unique_ptr<CacheStorageCacheHandle> cache_handle =
      GetLoadedCache(cache_name);
  if (cache_handle) {
    callback.Run(std::move(cache_handle), CACHE_STORAGE_OK);
    return;
  }

  cache_loader_->PrepareNewCacheDestination(
      cache_name, base::Bind(&CacheStorage::CreateCacheDidCreateCache,
                             weak_factory_.GetWeakPtr(), cache_name, callback));
}

void CacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    std::unique_ptr<CacheStorageCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  UMA_HISTOGRAM_BOOLEAN("ServiceWorkerCache.CreateCacheStorageResult",
                        static_cast<bool>(cache));

  if (!cache) {
    callback.Run(std::unique_ptr<CacheStorageCacheHandle>(),
                 CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  CacheStorageCache* cache_ptr = cache.get();

  cache_map_.insert(std::make_pair(cache_name, std::move(cache)));
  cache_index_->Insert(
      CacheStorageIndex::CacheMetadata(cache_name, cache_ptr->cache_size()));

  cache_loader_->WriteIndex(
      *cache_index_, base::Bind(&CacheStorage::CreateCacheDidWriteIndex,
                                weak_factory_.GetWeakPtr(), callback,
                                base::Passed(CreateCacheHandle(cache_ptr))));

  cache_loader_->NotifyCacheCreated(cache_name, CreateCacheHandle(cache_ptr));
}

void CacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_handle);

  // TODO(jkarlin): Handle !success.

  callback.Run(std::move(cache_handle), CACHE_STORAGE_OK);
}

void CacheStorage::HasCacheImpl(const std::string& cache_name,
                                const BoolAndErrorCallback& callback) {
  bool has_cache = base::ContainsKey(cache_map_, cache_name);
  callback.Run(has_cache, CACHE_STORAGE_OK);
}

void CacheStorage::DeleteCacheImpl(const std::string& cache_name,
                                   const BoolAndErrorCallback& callback) {
  std::unique_ptr<CacheStorageCacheHandle> cache_handle =
      GetLoadedCache(cache_name);
  if (!cache_handle) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, false, CACHE_STORAGE_ERROR_NOT_FOUND));
    return;
  }

  cache_handle->value()->SetObserver(nullptr);
  cache_index_->DoomCache(cache_name);
  cache_loader_->WriteIndex(
      *cache_index_,
      base::Bind(&CacheStorage::DeleteCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(cache_handle)), callback));
}

void CacheStorage::DeleteCacheDidWriteIndex(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    const BoolAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!success) {
    // Undo any changes if the index couldn't be written to disk.
    cache_index_->RestoreDoomedCache();
    cache_handle->value()->SetObserver(this);
    callback.Run(false, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  cache_index_->FinalizeDoomedCache();

  CacheMap::iterator map_iter =
      cache_map_.find(cache_handle->value()->cache_name());
  DCHECK(map_iter != cache_map_.end());

  doomed_caches_.insert(
      std::make_pair(map_iter->second.get(), std::move(map_iter->second)));
  cache_map_.erase(map_iter);

  cache_loader_->NotifyCacheDoomed(std::move(cache_handle));

  callback.Run(true, CACHE_STORAGE_OK);
}

// Call this once the last handle to a doomed cache is gone. It's okay if this
// doesn't get to complete before shutdown, the cache will be removed from disk
// on next startup in that case.
void CacheStorage::DeleteCacheFinalize(CacheStorageCache* doomed_cache) {
  doomed_cache->Size(base::Bind(&CacheStorage::DeleteCacheDidGetSize,
                                weak_factory_.GetWeakPtr(), doomed_cache));
}

void CacheStorage::DeleteCacheDidGetSize(CacheStorageCache* doomed_cache,
                                         int64_t cache_size) {
  quota_manager_proxy_->NotifyStorageModified(
      storage::QuotaClient::kServiceWorkerCache, origin_,
      storage::kStorageTypeTemporary, -1 * cache_size);

  cache_loader_->CleanUpDeletedCache(doomed_cache);
  auto doomed_caches_iter = doomed_caches_.find(doomed_cache);
  DCHECK(doomed_caches_iter != doomed_caches_.end());
  doomed_caches_.erase(doomed_caches_iter);
}

void CacheStorage::EnumerateCachesImpl(const IndexCallback& callback) {
  callback.Run(*cache_index_);
}

void CacheStorage::MatchCacheImpl(
    const std::string& cache_name,
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const CacheStorageCache::ResponseCallback& callback) {
  std::unique_ptr<CacheStorageCacheHandle> cache_handle =
      GetLoadedCache(cache_name);

  if (!cache_handle) {
    callback.Run(CACHE_STORAGE_ERROR_CACHE_NAME_NOT_FOUND,
                 std::unique_ptr<ServiceWorkerResponse>(),
                 std::unique_ptr<storage::BlobDataHandle>());
    return;
  }

  // Pass the cache handle along to the callback to keep the cache open until
  // match is done.
  CacheStorageCache* cache_ptr = cache_handle->value();
  cache_ptr->Match(
      std::move(request), match_params,
      base::Bind(&CacheStorage::MatchCacheDidMatch, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(cache_handle)), callback));
}

void CacheStorage::MatchCacheDidMatch(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    const CacheStorageCache::ResponseCallback& callback,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> response,
    std::unique_ptr<storage::BlobDataHandle> handle) {
  callback.Run(error, std::move(response), std::move(handle));
}

void CacheStorage::MatchAllCachesImpl(
    std::unique_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCacheQueryParams& match_params,
    const CacheStorageCache::ResponseCallback& callback) {
  std::vector<CacheMatchResponse>* match_responses =
      new std::vector<CacheMatchResponse>(cache_index_->num_entries());

  base::Closure barrier_closure = base::BarrierClosure(
      cache_index_->num_entries(),
      base::Bind(&CacheStorage::MatchAllCachesDidMatchAll,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(base::WrapUnique(match_responses)), callback));

  size_t idx = 0;
  for (const auto& cache_metadata : cache_index_->ordered_cache_metadata()) {
    std::unique_ptr<CacheStorageCacheHandle> cache_handle =
        GetLoadedCache(cache_metadata.name);
    DCHECK(cache_handle);

    CacheStorageCache* cache_ptr = cache_handle->value();
    cache_ptr->Match(base::MakeUnique<ServiceWorkerFetchRequest>(*request),
                     match_params,
                     base::Bind(&CacheStorage::MatchAllCachesDidMatch,
                                weak_factory_.GetWeakPtr(),
                                base::Passed(std::move(cache_handle)),
                                &match_responses->at(idx), barrier_closure));
    idx++;
  }
}

void CacheStorage::MatchAllCachesDidMatch(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    CacheMatchResponse* out_match_response,
    const base::Closure& barrier_closure,
    CacheStorageError error,
    std::unique_ptr<ServiceWorkerResponse> service_worker_response,
    std::unique_ptr<storage::BlobDataHandle> handle) {
  out_match_response->error = error;
  out_match_response->service_worker_response =
      std::move(service_worker_response);
  out_match_response->blob_data_handle = std::move(handle);
  barrier_closure.Run();
}

void CacheStorage::MatchAllCachesDidMatchAll(
    std::unique_ptr<std::vector<CacheMatchResponse>> match_responses,
    const CacheStorageCache::ResponseCallback& callback) {
  for (CacheMatchResponse& match_response : *match_responses) {
    if (match_response.error == CACHE_STORAGE_ERROR_NOT_FOUND)
      continue;
    callback.Run(match_response.error,
                 std::move(match_response.service_worker_response),
                 std::move(match_response.blob_data_handle));
    return;
  }
  callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
               std::unique_ptr<ServiceWorkerResponse>(),
               std::unique_ptr<storage::BlobDataHandle>());
}

void CacheStorage::AddCacheHandleRef(CacheStorageCache* cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = cache_handle_counts_.find(cache);
  if (iter == cache_handle_counts_.end()) {
    cache_handle_counts_[cache] = 1;
    return;
  }

  iter->second += 1;
}

void CacheStorage::DropCacheHandleRef(CacheStorageCache* cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto iter = cache_handle_counts_.find(cache);
  DCHECK(iter != cache_handle_counts_.end());
  DCHECK_GE(iter->second, 1U);

  iter->second -= 1;
  if (iter->second == 0) {
    cache_handle_counts_.erase(iter);
    auto doomed_caches_iter = doomed_caches_.find(cache);
    if (doomed_caches_iter != doomed_caches_.end()) {
      // The last reference to a doomed cache is gone, perform clean up.
      DeleteCacheFinalize(cache);
      return;
    }

    auto cache_map_iter = cache_map_.find(cache->cache_name());
    DCHECK(cache_map_iter != cache_map_.end());

    cache_map_iter->second.reset();
  }
}

std::unique_ptr<CacheStorageCacheHandle> CacheStorage::CreateCacheHandle(
    CacheStorageCache* cache) {
  DCHECK(cache);
  return std::unique_ptr<CacheStorageCacheHandle>(new CacheStorageCacheHandle(
      cache->AsWeakPtr(), weak_factory_.GetWeakPtr()));
}

std::unique_ptr<CacheStorageCacheHandle> CacheStorage::GetLoadedCache(
    const std::string& cache_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  CacheMap::iterator map_iter = cache_map_.find(cache_name);
  if (map_iter == cache_map_.end())
    return std::unique_ptr<CacheStorageCacheHandle>();

  CacheStorageCache* cache = map_iter->second.get();

  if (!cache) {
    std::unique_ptr<CacheStorageCache> new_cache = cache_loader_->CreateCache(
        cache_name, cache_index_->GetCacheSize(cache_name));
    CacheStorageCache* cache_ptr = new_cache.get();
    map_iter->second = std::move(new_cache);

    return CreateCacheHandle(cache_ptr);
  }

  return CreateCacheHandle(cache);
}

void CacheStorage::SizeRetrievedFromCache(
    std::unique_ptr<CacheStorageCacheHandle> cache_handle,
    const base::Closure& closure,
    int64_t* accumulator,
    int64_t size) {
  cache_index_->SetCacheSize(cache_handle->value()->cache_name(), size);
  *accumulator += size;
  closure.Run();
}

void CacheStorage::GetSizeThenCloseAllCachesImpl(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  std::unique_ptr<int64_t> accumulator(new int64_t(0));
  int64_t* accumulator_ptr = accumulator.get();

  base::Closure barrier_closure = base::BarrierClosure(
      cache_index_->num_entries(),
      base::Bind(&SizeRetrievedFromAllCaches,
                 base::Passed(std::move(accumulator)), callback));

  for (const auto& cache_metadata : cache_index_->ordered_cache_metadata()) {
    auto cache_handle = GetLoadedCache(cache_metadata.name);
    CacheStorageCache* cache = cache_handle->value();
    cache->GetSizeThenClose(base::Bind(&CacheStorage::SizeRetrievedFromCache,
                                       weak_factory_.GetWeakPtr(),
                                       base::Passed(std::move(cache_handle)),
                                       barrier_closure, accumulator_ptr));
  }
}

void CacheStorage::SizeImpl(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  if (cache_index_->GetStorageSize() != kSizeUnknown) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback, cache_index_->GetStorageSize()));
    return;
  }

  std::unique_ptr<int64_t> accumulator(new int64_t(0));
  int64_t* accumulator_ptr = accumulator.get();

  base::Closure barrier_closure = base::BarrierClosure(
      cache_index_->num_entries(),
      base::Bind(&SizeRetrievedFromAllCaches,
                 base::Passed(std::move(accumulator)), callback));

  for (const auto& cache_metadata : cache_index_->ordered_cache_metadata()) {
    if (cache_metadata.size != CacheStorage::kSizeUnknown) {
      *accumulator_ptr += cache_metadata.size;
      barrier_closure.Run();
      continue;
    }
    std::unique_ptr<CacheStorageCacheHandle> cache_handle =
        GetLoadedCache(cache_metadata.name);
    CacheStorageCache* cache = cache_handle->value();
    cache->Size(base::Bind(&CacheStorage::SizeRetrievedFromCache,
                           weak_factory_.GetWeakPtr(),
                           base::Passed(std::move(cache_handle)),
                           barrier_closure, accumulator_ptr));
  }
}

}  // namespace content
