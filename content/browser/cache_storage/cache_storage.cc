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
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/sha1.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/thread_task_runner_handle.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_scheduler.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/quota/quota_manager_proxy.h"

namespace content {

namespace {

const int kCachePreservationInSecs = 30;

std::string HexedHash(const std::string& value) {
  std::string value_hash = base::SHA1HashString(value);
  std::string valued_hexed_hash = base::ToLowerASCII(
      base::HexEncode(value_hash.c_str(), value_hash.length()));
  return valued_hexed_hash;
}

void CloseAllCachesDidCloseCache(const scoped_refptr<CacheStorageCache>& cache,
                                 const base::Closure& barrier_closure) {
  barrier_closure.Run();
}

void SizeRetrievedFromCache(const scoped_refptr<CacheStorageCache>& cache,
                            const base::Closure& closure,
                            int64_t* accumulator,
                            int64_t size) {
  *accumulator += size;
  closure.Run();
}

void SizeRetrievedFromAllCaches(scoped_ptr<int64_t> accumulator,
                                const CacheStorage::SizeCallback& callback) {
  callback.Run(*accumulator);
}

}  // namespace

const char CacheStorage::kIndexFileName[] = "index.txt";

// Handles the loading and clean up of CacheStorageCache objects.
class CacheStorage::CacheLoader {
 public:
  typedef base::Callback<void(const scoped_refptr<CacheStorageCache>&)>
      CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(scoped_ptr<std::vector<std::string>>)>
      StringVectorCallback;

  CacheLoader(
      base::SequencedTaskRunner* cache_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin)
      : cache_task_runner_(cache_task_runner),
        request_context_getter_(request_context_getter),
        quota_manager_proxy_(quota_manager_proxy),
        blob_context_(blob_context),
        origin_(origin) {
    DCHECK(!origin_.is_empty());
  }

  virtual ~CacheLoader() {}

  // Creates a CacheStorageCache with the given name. It does not attempt to
  // load the backend, that happens lazily when the cache is used.
  virtual scoped_refptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) = 0;

  // Deletes any pre-existing cache of the same name and then loads it.
  virtual void PrepareNewCacheDestination(const std::string& cache_name,
                                          const CacheCallback& callback) = 0;

  // After the backend has been deleted, do any extra house keeping such as
  // removing the cache's directory.
  virtual void CleanUpDeletedCache(const std::string& key,
                                   const BoolCallback& callback) = 0;

  // Writes the cache names (and sizes) to disk if applicable.
  virtual void WriteIndex(const StringVector& cache_names,
                          const BoolCallback& callback) = 0;

  // Loads the cache names from disk if applicable.
  virtual void LoadIndex(scoped_ptr<std::vector<std::string>> cache_names,
                         const StringVectorCallback& callback) = 0;

 protected:
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::WeakPtr<storage::BlobStorageContext> blob_context_;
  GURL origin_;
};

// Creates memory-only ServiceWorkerCaches. Because these caches have no
// persistent storage it is not safe to free them from memory if they might be
// used again. Therefore this class holds a reference to each cache until the
// cache is deleted.
class CacheStorage::MemoryLoader : public CacheStorage::CacheLoader {
 public:
  MemoryLoader(
      base::SequencedTaskRunner* cache_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    origin) {}

  scoped_refptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) override {
    return CacheStorageCache::CreateMemoryCache(
        origin_, request_context_getter_, quota_manager_proxy_, blob_context_);
  }

  void PrepareNewCacheDestination(const std::string& cache_name,
                                  const CacheCallback& callback) override {
    scoped_refptr<CacheStorageCache> cache = CreateCache(cache_name);
    cache_refs_.insert(std::make_pair(cache_name, cache));
    callback.Run(cache);
  }

  void CleanUpDeletedCache(const std::string& cache_name,
                           const BoolCallback& callback) override {
    CacheRefMap::iterator it = cache_refs_.find(cache_name);
    DCHECK(it != cache_refs_.end());
    cache_refs_.erase(it);
    callback.Run(true);
  }

  void WriteIndex(const StringVector& cache_names,
                  const BoolCallback& callback) override {
    callback.Run(false);
  }

  void LoadIndex(scoped_ptr<std::vector<std::string>> cache_names,
                 const StringVectorCallback& callback) override {
    callback.Run(std::move(cache_names));
  }

 private:
  typedef std::map<std::string, scoped_refptr<CacheStorageCache>> CacheRefMap;
  ~MemoryLoader() override {}

  // Keep a reference to each cache to ensure that it's not freed before the
  // client calls CacheStorage::Delete or the CacheStorage is
  // freed.
  CacheRefMap cache_refs_;
};

class CacheStorage::SimpleCacheLoader : public CacheStorage::CacheLoader {
 public:
  SimpleCacheLoader(
      const base::FilePath& origin_path,
      base::SequencedTaskRunner* cache_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>& request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin)
      : CacheLoader(cache_task_runner,
                    request_context,
                    quota_manager_proxy,
                    blob_context,
                    origin),
        origin_path_(origin_path),
        weak_ptr_factory_(this) {}

  scoped_refptr<CacheStorageCache> CreateCache(
      const std::string& cache_name) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_name));

    std::string cache_dir = cache_name_to_cache_dir_[cache_name];
    base::FilePath cache_path = origin_path_.AppendASCII(cache_dir);
    return CacheStorageCache::CreatePersistentCache(
        origin_, cache_path, request_context_getter_, quota_manager_proxy_,
        blob_context_);
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
      callback.Run(scoped_refptr<CacheStorageCache>());
      return;
    }

    cache_name_to_cache_dir_[cache_name] = cache_dir;
    callback.Run(CreateCache(cache_name));
  }

  void CleanUpDeletedCache(const std::string& cache_name,
                           const BoolCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_name));

    base::FilePath cache_path =
        origin_path_.AppendASCII(cache_name_to_cache_dir_[cache_name]);
    cache_name_to_cache_dir_.erase(cache_name);

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::CleanUpDeleteCacheDirInPool, cache_path,
                   callback, base::ThreadTaskRunnerHandle::Get()));
  }

  static void CleanUpDeleteCacheDirInPool(
      const base::FilePath& cache_path,
      const BoolCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& original_task_runner) {
    bool rv = base::DeleteFile(cache_path, true);
    original_task_runner->PostTask(FROM_HERE, base::Bind(callback, rv));
  }

  void WriteIndex(const StringVector& cache_names,
                  const BoolCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    CacheStorageIndex index;
    index.set_origin(origin_.spec());

    for (size_t i = 0u, max = cache_names.size(); i < max; ++i) {
      DCHECK(ContainsKey(cache_name_to_cache_dir_, cache_names[i]));

      CacheStorageIndex::Cache* index_cache = index.add_cache();
      index_cache->set_name(cache_names[i]);
      index_cache->set_cache_dir(cache_name_to_cache_dir_[cache_names[i]]);
    }

    std::string serialized;
    bool success = index.SerializeToString(&serialized);
    DCHECK(success);

    base::FilePath tmp_path = origin_path_.AppendASCII("index.txt.tmp");
    base::FilePath index_path =
        origin_path_.AppendASCII(CacheStorage::kIndexFileName);

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&SimpleCacheLoader::WriteIndexWriteToFileInPool,
                              tmp_path, index_path, serialized, callback,
                              base::ThreadTaskRunnerHandle::Get()));
  }

  static void WriteIndexWriteToFileInPool(
      const base::FilePath& tmp_path,
      const base::FilePath& index_path,
      const std::string& data,
      const BoolCallback& callback,
      const scoped_refptr<base::SingleThreadTaskRunner>& original_task_runner) {
    int bytes_written = base::WriteFile(tmp_path, data.c_str(), data.size());
    if (bytes_written != base::checked_cast<int>(data.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      original_task_runner->PostTask(FROM_HERE, base::Bind(callback, false));
    }

    // Atomically rename the temporary index file to become the real one.
    bool rv = base::ReplaceFile(tmp_path, index_path, NULL);
    original_task_runner->PostTask(FROM_HERE, base::Bind(callback, rv));
  }

  void LoadIndex(scoped_ptr<std::vector<std::string>> names,
                 const StringVectorCallback& callback) override {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Read the file from disk. (LoadIndexReadFileInPool)
    // 2. Parse file and return the names of the caches (LoadIndexDidReadFile)

    base::FilePath index_path =
        origin_path_.AppendASCII(CacheStorage::kIndexFileName);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(), FROM_HERE,
        base::Bind(&SimpleCacheLoader::ReadAndMigrateIndexInPool, index_path),
        base::Bind(&SimpleCacheLoader::LoadIndexDidReadFile,
                   weak_ptr_factory_.GetWeakPtr(), base::Passed(&names),
                   callback));
  }

  void LoadIndexDidReadFile(scoped_ptr<std::vector<std::string>> names,
                            const StringVectorCallback& callback,
                            const std::string& serialized) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    scoped_ptr<std::set<std::string>> cache_dirs(new std::set<std::string>);

    CacheStorageIndex index;
    if (index.ParseFromString(serialized)) {
      for (int i = 0, max = index.cache_size(); i < max; ++i) {
        const CacheStorageIndex::Cache& cache = index.cache(i);
        DCHECK(cache.has_cache_dir());
        names->push_back(cache.name());
        cache_name_to_cache_dir_[cache.name()] = cache.cache_dir();
        cache_dirs->insert(cache.cache_dir());
      }
    }

    cache_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DeleteUnreferencedCachesInPool, origin_path_,
                              base::Passed(&cache_dirs)));
    callback.Run(std::move(names));
  }

 private:
  friend class MigratedLegacyCacheDirectoryNameTest;
  ~SimpleCacheLoader() override {}

  // Iterates over the caches and deletes any directory not found in
  // |cache_dirs|. Runs on cache_task_runner_
  static void DeleteUnreferencedCachesInPool(
      const base::FilePath& cache_base_dir,
      scoped_ptr<std::set<std::string>> cache_dirs) {
    base::FileEnumerator file_enum(cache_base_dir, false /* recursive */,
                                   base::FileEnumerator::DIRECTORIES);
    std::vector<base::FilePath> dirs_to_delete;
    base::FilePath cache_path;
    while (!(cache_path = file_enum.Next()).empty()) {
      if (!ContainsKey(*cache_dirs, cache_path.BaseName().AsUTF8Unsafe()))
        dirs_to_delete.push_back(cache_path);
    }

    for (const base::FilePath& cache_path : dirs_to_delete)
      base::DeleteFile(cache_path, true /* recursive */);
  }

  // Runs on cache_task_runner_
  static std::string MigrateCachesIfNecessaryInPool(
      const std::string& body,
      const base::FilePath& index_path) {
    CacheStorageIndex index;
    if (!index.ParseFromString(body))
      return body;

    base::FilePath origin_path = index_path.DirName();
    bool index_is_dirty = false;
    const std::string kBadIndexState("");

    // Look for caches that have no cache_dir. Give any such caches a directory
    // with a random name and move them there. Then, rewrite the index file.
    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const CacheStorageIndex::Cache& cache = index.cache(i);
      if (!cache.has_cache_dir()) {
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
          return kBadIndexState;
        }

        index.mutable_cache(i)->set_cache_dir(cache_dir);
        index_is_dirty = true;
      }
    }

    if (index_is_dirty) {
      std::string new_body;
      if (!index.SerializeToString(&new_body))
        return kBadIndexState;
      if (base::WriteFile(index_path, new_body.c_str(), new_body.size()) !=
          base::checked_cast<int>(new_body.size()))
        return kBadIndexState;
      return new_body;
    }

    return body;
  }

  // Runs on cache_task_runner_
  static std::string ReadAndMigrateIndexInPool(
      const base::FilePath& index_path) {
    std::string body;
    base::ReadFileToString(index_path, &body);

    return MigrateCachesIfNecessaryInPool(body, index_path);
  }

  const base::FilePath origin_path_;
  std::map<std::string, std::string> cache_name_to_cache_dir_;

  base::WeakPtrFactory<SimpleCacheLoader> weak_ptr_factory_;
};

CacheStorage::CacheStorage(
    const base::FilePath& path,
    bool memory_only,
    base::SequencedTaskRunner* cache_task_runner,
    const scoped_refptr<net::URLRequestContextGetter>& request_context,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
    base::WeakPtr<storage::BlobStorageContext> blob_context,
    const GURL& origin)
    : initialized_(false),
      initializing_(false),
      scheduler_(new CacheStorageScheduler()),
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      memory_only_(memory_only),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader(cache_task_runner_.get(),
                                         request_context, quota_manager_proxy,
                                         blob_context, origin));
  else
    cache_loader_.reset(new SimpleCacheLoader(
        origin_path_, cache_task_runner_.get(), request_context,
        quota_manager_proxy, blob_context, origin));
}

CacheStorage::~CacheStorage() {
}

void CacheStorage::OpenCache(const std::string& cache_name,
                             const CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingCacheAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::OpenCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::HasCache(const std::string& cache_name,
                            const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  BoolAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingBoolAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::HasCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::DeleteCache(const std::string& cache_name,
                               const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  BoolAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingBoolAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::DeleteCacheImpl,
                                           weak_factory_.GetWeakPtr(),
                                           cache_name, pending_callback));
}

void CacheStorage::EnumerateCaches(const StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  StringsAndErrorCallback pending_callback =
      base::Bind(&CacheStorage::PendingStringsAndErrorCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::EnumerateCachesImpl,
                                           weak_factory_.GetWeakPtr(),
                                           pending_callback));
}

void CacheStorage::MatchCache(
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheStorageCache::ResponseCallback pending_callback =
      base::Bind(&CacheStorage::PendingResponseCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorage::MatchCacheImpl, weak_factory_.GetWeakPtr(), cache_name,
      base::Passed(std::move(request)), pending_callback));
}

void CacheStorage::MatchAllCaches(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheStorageCache::ResponseCallback pending_callback =
      base::Bind(&CacheStorage::PendingResponseCallback,
                 weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(
      base::Bind(&CacheStorage::MatchAllCachesImpl, weak_factory_.GetWeakPtr(),
                 base::Passed(std::move(request)), pending_callback));
}

void CacheStorage::CloseAllCaches(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_) {
    callback.Run();
    return;
  }

  base::Closure pending_callback = base::Bind(
      &CacheStorage::PendingClosure, weak_factory_.GetWeakPtr(), callback);
  scheduler_->ScheduleOperation(base::Bind(&CacheStorage::CloseAllCachesImpl,
                                           weak_factory_.GetWeakPtr(),
                                           pending_callback));
}

void CacheStorage::Size(const CacheStorage::SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_)
    LazyInit();

  CacheStorageCache::SizeCallback pending_callback = base::Bind(
      &CacheStorage::PendingSizeCallback, weak_factory_.GetWeakPtr(), callback);

  scheduler_->ScheduleOperation(base::Bind(
      &CacheStorage::SizeImpl, weak_factory_.GetWeakPtr(), pending_callback));
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

  // 1. Get the list of cache names (async call)
  // 2. For each cache name, load the cache (async call)
  // 3. Once each load is complete, update the map variables.
  // 4. Call the list of waiting callbacks.

  scoped_ptr<std::vector<std::string>> indexed_cache_names(
      new std::vector<std::string>());

  cache_loader_->LoadIndex(std::move(indexed_cache_names),
                           base::Bind(&CacheStorage::LazyInitDidLoadIndex,
                                      weak_factory_.GetWeakPtr()));
}

void CacheStorage::LazyInitDidLoadIndex(
    scoped_ptr<std::vector<std::string>> indexed_cache_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  for (size_t i = 0u, max = indexed_cache_names->size(); i < max; ++i) {
    cache_map_.insert(std::make_pair(indexed_cache_names->at(i),
                                     base::WeakPtr<CacheStorageCache>()));
    ordered_cache_names_.push_back(indexed_cache_names->at(i));
  }

  initializing_ = false;
  initialized_ = true;

  scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::OpenCacheImpl(const std::string& cache_name,
                                 const CacheAndErrorCallback& callback) {
  scoped_refptr<CacheStorageCache> cache = GetLoadedCache(cache_name);
  if (cache.get()) {
    callback.Run(cache, CACHE_STORAGE_OK);
    return;
  }

  cache_loader_->PrepareNewCacheDestination(
      cache_name, base::Bind(&CacheStorage::CreateCacheDidCreateCache,
                             weak_factory_.GetWeakPtr(), cache_name, callback));
}

void CacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    const scoped_refptr<CacheStorageCache>& cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  UMA_HISTOGRAM_BOOLEAN("ServiceWorkerCache.CreateCacheStorageResult",
                        cache != nullptr);

  if (!cache.get()) {
    callback.Run(scoped_refptr<CacheStorageCache>(),
                 CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  cache_map_.insert(std::make_pair(cache_name, cache->AsWeakPtr()));
  ordered_cache_names_.push_back(cache_name);

  TemporarilyPreserveCache(cache);

  cache_loader_->WriteIndex(
      ordered_cache_names_,
      base::Bind(&CacheStorage::CreateCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(), callback, cache));
}

void CacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    const scoped_refptr<CacheStorageCache>& cache,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache.get());

  // TODO(jkarlin): Handle !success.

  callback.Run(cache, CACHE_STORAGE_OK);
}

void CacheStorage::HasCacheImpl(const std::string& cache_name,
                                const BoolAndErrorCallback& callback) {
  bool has_cache = ContainsKey(cache_map_, cache_name);
  callback.Run(has_cache, CACHE_STORAGE_OK);
}

void CacheStorage::DeleteCacheImpl(const std::string& cache_name,
                                   const BoolAndErrorCallback& callback) {
  CacheMap::iterator it = cache_map_.find(cache_name);
  if (it == cache_map_.end()) {
    callback.Run(false, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  base::WeakPtr<CacheStorageCache> cache = it->second;
  cache_map_.erase(it);

  // Delete the name from ordered_cache_names_.
  StringVector::iterator iter = std::find(
      ordered_cache_names_.begin(), ordered_cache_names_.end(), cache_name);
  DCHECK(iter != ordered_cache_names_.end());
  ordered_cache_names_.erase(iter);

  base::Closure closure =
      base::Bind(&CacheStorage::DeleteCacheDidClose, weak_factory_.GetWeakPtr(),
                 cache_name, callback, ordered_cache_names_,
                 make_scoped_refptr(cache.get()));

  if (cache) {
    cache->Close(closure);
    return;
  }

  closure.Run();
}

void CacheStorage::DeleteCacheDidClose(
    const std::string& cache_name,
    const BoolAndErrorCallback& callback,
    const StringVector& ordered_cache_names,
    const scoped_refptr<CacheStorageCache>& cache /* might be null */) {
  cache_loader_->WriteIndex(
      ordered_cache_names,
      base::Bind(&CacheStorage::DeleteCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(), cache_name, callback));
}

void CacheStorage::DeleteCacheDidWriteIndex(
    const std::string& cache_name,
    const BoolAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  cache_loader_->CleanUpDeletedCache(
      cache_name, base::Bind(&CacheStorage::DeleteCacheDidCleanUp,
                             weak_factory_.GetWeakPtr(), callback));
}

void CacheStorage::DeleteCacheDidCleanUp(const BoolAndErrorCallback& callback,
                                         bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(true, CACHE_STORAGE_OK);
}

void CacheStorage::EnumerateCachesImpl(
    const StringsAndErrorCallback& callback) {
  callback.Run(ordered_cache_names_, CACHE_STORAGE_OK);
}

void CacheStorage::MatchCacheImpl(
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  scoped_refptr<CacheStorageCache> cache = GetLoadedCache(cache_name);

  if (!cache.get()) {
    callback.Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                 scoped_ptr<ServiceWorkerResponse>(),
                 scoped_ptr<storage::BlobDataHandle>());
    return;
  }

  // Pass the cache along to the callback to keep the cache open until match is
  // done.
  cache->Match(std::move(request),
               base::Bind(&CacheStorage::MatchCacheDidMatch,
                          weak_factory_.GetWeakPtr(), cache, callback));
}

void CacheStorage::MatchCacheDidMatch(
    const scoped_refptr<CacheStorageCache>& cache,
    const CacheStorageCache::ResponseCallback& callback,
    CacheStorageError error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> handle) {
  callback.Run(error, std::move(response), std::move(handle));
}

void CacheStorage::MatchAllCachesImpl(
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  scoped_ptr<CacheStorageCache::ResponseCallback> callback_copy(
      new CacheStorageCache::ResponseCallback(callback));

  CacheStorageCache::ResponseCallback* callback_ptr = callback_copy.get();
  base::Closure barrier_closure =
      base::BarrierClosure(ordered_cache_names_.size(),
                           base::Bind(&CacheStorage::MatchAllCachesDidMatchAll,
                                      weak_factory_.GetWeakPtr(),
                                      base::Passed(std::move(callback_copy))));

  for (const std::string& cache_name : ordered_cache_names_) {
    scoped_refptr<CacheStorageCache> cache = GetLoadedCache(cache_name);
    DCHECK(cache.get());

    cache->Match(make_scoped_ptr(new ServiceWorkerFetchRequest(*request)),
                 base::Bind(&CacheStorage::MatchAllCachesDidMatch,
                            weak_factory_.GetWeakPtr(), cache, barrier_closure,
                            callback_ptr));
  }
}

void CacheStorage::MatchAllCachesDidMatch(
    scoped_refptr<CacheStorageCache> cache,
    const base::Closure& barrier_closure,
    CacheStorageCache::ResponseCallback* callback,
    CacheStorageError error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> handle) {
  if (callback->is_null() || error == CACHE_STORAGE_ERROR_NOT_FOUND) {
    barrier_closure.Run();
    return;
  }
  callback->Run(error, std::move(response), std::move(handle));
  callback->Reset();  // Only call the callback once.

  barrier_closure.Run();
}

void CacheStorage::MatchAllCachesDidMatchAll(
    scoped_ptr<CacheStorageCache::ResponseCallback> callback) {
  if (!callback->is_null()) {
    callback->Run(CACHE_STORAGE_ERROR_NOT_FOUND,
                  scoped_ptr<ServiceWorkerResponse>(),
                  scoped_ptr<storage::BlobDataHandle>());
  }
}

scoped_refptr<CacheStorageCache> CacheStorage::GetLoadedCache(
    const std::string& cache_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  CacheMap::iterator map_iter = cache_map_.find(cache_name);
  if (map_iter == cache_map_.end())
    return scoped_refptr<CacheStorageCache>();

  base::WeakPtr<CacheStorageCache> cache = map_iter->second;

  if (!cache) {
    scoped_refptr<CacheStorageCache> new_cache =
        cache_loader_->CreateCache(cache_name);
    map_iter->second = new_cache->AsWeakPtr();

    TemporarilyPreserveCache(new_cache);
    return new_cache;
  }

  return make_scoped_refptr(cache.get());
}

void CacheStorage::TemporarilyPreserveCache(
    const scoped_refptr<CacheStorageCache>& cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!ContainsKey(preserved_caches_, cache.get()));

  preserved_caches_[cache.get()] = cache;
  SchedulePreservedCacheRemoval(base::Bind(&CacheStorage::RemovePreservedCache,
                                           weak_factory_.GetWeakPtr(),
                                           base::Unretained(cache.get())));
}

void CacheStorage::SchedulePreservedCacheRemoval(
    const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, callback,
      base::TimeDelta::FromSeconds(kCachePreservationInSecs));
}

void CacheStorage::RemovePreservedCache(const CacheStorageCache* cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ContainsKey(preserved_caches_, cache));

  preserved_caches_.erase(cache);
}

void CacheStorage::CloseAllCachesImpl(const base::Closure& callback) {
  int live_cache_count = 0;
  for (const auto& key_value : cache_map_) {
    if (key_value.second)
      live_cache_count += 1;
  }

  if (live_cache_count == 0) {
    callback.Run();
    return;
  }

  // The closure might modify this object so delay calling it until after
  // iterating through cache_map_ by adding one to the barrier.
  base::Closure barrier_closure =
      base::BarrierClosure(live_cache_count + 1, base::Bind(callback));

  for (auto& key_value : cache_map_) {
    if (key_value.second) {
      key_value.second->Close(base::Bind(
          CloseAllCachesDidCloseCache,
          make_scoped_refptr(key_value.second.get()), barrier_closure));
    }
  }

  barrier_closure.Run();
}

void CacheStorage::SizeImpl(const SizeCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  scoped_ptr<int64_t> accumulator(new int64_t(0));
  int64_t* accumulator_ptr = accumulator.get();

  base::Closure barrier_closure = base::BarrierClosure(
      ordered_cache_names_.size(),
      base::Bind(&SizeRetrievedFromAllCaches,
                 base::Passed(std::move(accumulator)), callback));

  for (const std::string& cache_name : ordered_cache_names_) {
    scoped_refptr<CacheStorageCache> cache = GetLoadedCache(cache_name);
    cache->Size(base::Bind(&SizeRetrievedFromCache, cache, barrier_closure,
                           accumulator_ptr));
  }
}

void CacheStorage::PendingClosure(const base::Closure& callback) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run();
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingBoolAndErrorCallback(
    const BoolAndErrorCallback& callback,
    bool found,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(found, error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingCacheAndErrorCallback(
    const CacheAndErrorCallback& callback,
    const scoped_refptr<CacheStorageCache>& cache,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(cache, error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingStringsAndErrorCallback(
    const StringsAndErrorCallback& callback,
    const StringVector& strings,
    CacheStorageError error) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(strings, error);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingResponseCallback(
    const CacheStorageCache::ResponseCallback& callback,
    CacheStorageError error,
    scoped_ptr<ServiceWorkerResponse> response,
    scoped_ptr<storage::BlobDataHandle> blob_data_handle) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(error, std::move(response), std::move(blob_data_handle));
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

void CacheStorage::PendingSizeCallback(const SizeCallback& callback,
                                       int64_t size) {
  base::WeakPtr<CacheStorage> cache_storage = weak_factory_.GetWeakPtr();

  callback.Run(size);
  if (cache_storage)
    scheduler_->CompleteOperationAndRunNext();
}

}  // namespace content
