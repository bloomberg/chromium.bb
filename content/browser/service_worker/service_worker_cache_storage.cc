// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage.h"

#include <string>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "webkit/browser/blob/blob_storage_context.h"

namespace content {

// static
const int ServiceWorkerCacheStorage::kInvalidCacheID = -1;

// The meta information related to each ServiceWorkerCache that the
// ServiceWorkerCacheManager needs to keep track of.
// TODO(jkarlin): Add reference counting so that the deletion of javascript
// objects can delete the ServiceWorkerCache.
struct ServiceWorkerCacheStorage::CacheContext {
  CacheContext(const std::string& name,
               CacheID id,
               scoped_ptr<ServiceWorkerCache> cache)
      : name(name), id(id), cache(cache.Pass()) {}
  std::string name;
  CacheID id;
  scoped_ptr<ServiceWorkerCache> cache;
};

// Handles the loading and clean up of ServiceWorkerCache objects. The
// callback of every public method is guaranteed to be called.
class ServiceWorkerCacheStorage::CacheLoader {
 public:
  typedef base::Callback<void(scoped_ptr<ServiceWorkerCache>)> CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(scoped_ptr<std::vector<std::string> >)>
      StringsCallback;

  CacheLoader(base::SequencedTaskRunner* cache_task_runner,
              net::URLRequestContext* request_context,
              base::WeakPtr<storage::BlobStorageContext> blob_context)
      : cache_task_runner_(cache_task_runner),
        request_context_(request_context),
        blob_context_(blob_context) {}

  virtual ~CacheLoader() {}

  // Loads the given cache_name, the cache is NULL if it fails. If the cache
  // doesn't exist a new one is created.
  virtual void LoadCache(const std::string& cache_name,
                         const CacheCallback& callback) = 0;

  // Deletes any pre-existing cache of the same name and then loads it.
  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) = 0;

  // After the backend has been deleted, do any extra house keeping such as
  // removing the cache's directory.
  virtual void CleanUpDeletedCache(const std::string& key,
                                   const BoolCallback& callback) = 0;

  // Writes the cache names (and sizes) to disk if applicable.
  virtual void WriteIndex(const CacheMap& caches,
                          const BoolCallback& callback) = 0;

  // Loads the cache names from disk if applicable.
  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > cache_names,
                         const StringsCallback& callback) = 0;

 protected:
  virtual void LoadCacheImpl(const std::string&) {}

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  net::URLRequestContext* request_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_context_;
};

class ServiceWorkerCacheStorage::MemoryLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  MemoryLoader(base::SequencedTaskRunner* cache_task_runner,
               net::URLRequestContext* request_context,
               base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheLoader(cache_task_runner, request_context, blob_context) {}
  virtual void LoadCache(const std::string& cache_name,
                         const CacheCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) OVERRIDE {
    scoped_ptr<ServiceWorkerCache> cache =
        ServiceWorkerCache::CreateMemoryCache(request_context_, blob_context_);
    callback.Run(cache.Pass());
  }

  virtual void CleanUpDeletedCache(const std::string& cache_name,
                                   const BoolCallback& callback) OVERRIDE {
    callback.Run(true);
  }

  virtual void WriteIndex(const CacheMap& caches,
                          const BoolCallback& callback) OVERRIDE {
    callback.Run(false);
  }

  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > cache_names,
                         const StringsCallback& callback) OVERRIDE {
    callback.Run(cache_names.Pass());
  }

 private:
  virtual ~MemoryLoader() {}
};

class ServiceWorkerCacheStorage::SimpleCacheLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  SimpleCacheLoader(const base::FilePath& origin_path,
                    base::SequencedTaskRunner* cache_task_runner,
                    net::URLRequestContext* request_context,
                    base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheLoader(cache_task_runner, request_context, blob_context),
        origin_path_(origin_path),
        weak_ptr_factory_(this) {}

  virtual void LoadCache(const std::string& cache_name,
                         const CacheCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the cache's directory if necessary. (LoadCreateDirectoryInPool)
    // 2. Create the cache object. (LoadDidCreateDirectory)

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadCreateDirectoryInPool,
                   CreatePersistentCachePath(origin_path_, cache_name),
                   cache_name,
                   callback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::MessageLoopProxy::current()));
  }

  static void LoadCreateDirectoryInPool(
      const base::FilePath& path,
      const std::string& cache_name,
      const CacheCallback& callback,
      base::WeakPtr<SimpleCacheLoader> loader,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    bool rv = base::CreateDirectory(path);
    original_loop->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadDidCreateDirectory,
                   cache_name,
                   callback,
                   loader,
                   rv));
  }

  static void LoadDidCreateDirectory(const std::string& cache_name,
                                     const CacheCallback& callback,
                                     base::WeakPtr<SimpleCacheLoader> loader,
                                     bool dir_rv) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (!dir_rv || !loader) {
      callback.Run(scoped_ptr<ServiceWorkerCache>());
      return;
    }

    scoped_ptr<ServiceWorkerCache> cache =
        ServiceWorkerCache::CreatePersistentCache(
            CreatePersistentCachePath(loader->origin_path_, cache_name),
            loader->request_context_,
            loader->blob_context_);
    callback.Run(cache.Pass());
  }

  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Delete the cache's directory if it exists.
    // (CreateCacheDeleteFilesInPool)
    // 2. Load the cache. (LoadCreateDirectoryInPool)

    base::FilePath cache_path =
        CreatePersistentCachePath(origin_path_, cache_name);

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::CreateCacheDeleteFilesInPool,
                   cache_path,
                   cache_name,
                   callback,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::MessageLoopProxy::current()));
  }

  static void CreateCacheDeleteFilesInPool(
      const base::FilePath& cache_path,
      const std::string& cache_name,
      const CacheCallback& callback,
      base::WeakPtr<SimpleCacheLoader> loader,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    base::FilePath path(cache_path);
    if (base::PathExists(path))
      base::DeleteFile(path, /* recursive */ true);

    // Jump straight into LoadCache on the same thread.
    LoadCreateDirectoryInPool(
        cache_path, cache_name, callback, loader, original_loop);
  }

  virtual void CleanUpDeletedCache(const std::string& cache_name,
                                   const BoolCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Delete the cache's directory. (CleanUpDeleteCacheDirInPool)

    base::FilePath cache_path =
        CreatePersistentCachePath(origin_path_, cache_name);
    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::CleanUpDeleteCacheDirInPool,
                   cache_path,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  static void CleanUpDeleteCacheDirInPool(
      const base::FilePath& cache_path,
      const BoolCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    bool rv = base::DeleteFile(cache_path, true);
    original_loop->PostTask(FROM_HERE, base::Bind(callback, rv));
  }

  virtual void WriteIndex(const CacheMap& caches,
                          const BoolCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    ServiceWorkerCacheStorageIndex index;

    for (CacheMap::const_iterator it = caches.begin(); it != caches.end();
         ++it) {
      const CacheContext* cache = it->second;
      ServiceWorkerCacheStorageIndex::Cache* index_cache = index.add_cache();
      index_cache->set_name(cache->name);
      index_cache->set_size(0);  // TODO(jkarlin): Make this real.
    }

    std::string serialized;
    bool success = index.SerializeToString(&serialized);
    DCHECK(success);

    base::FilePath tmp_path = origin_path_.AppendASCII("index.txt.tmp");
    base::FilePath index_path = origin_path_.AppendASCII("index.txt");

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::WriteIndexWriteToFileInPool,
                   tmp_path,
                   index_path,
                   serialized,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  static void WriteIndexWriteToFileInPool(
      const base::FilePath& tmp_path,
      const base::FilePath& index_path,
      const std::string& data,
      const BoolCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    int bytes_written = base::WriteFile(tmp_path, data.c_str(), data.size());
    if (bytes_written != implicit_cast<int>(data.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      original_loop->PostTask(FROM_HERE, base::Bind(callback, false));
    }

    // Atomically rename the temporary index file to become the real one.
    bool rv = base::ReplaceFile(tmp_path, index_path, NULL);
    original_loop->PostTask(FROM_HERE, base::Bind(callback, rv));
  }

  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > names,
                         const StringsCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Read the file from disk. (LoadIndexReadFileInPool)
    // 2. Parse file and return the names of the caches (LoadIndexDidReadFile)

    base::FilePath index_path = origin_path_.AppendASCII("index.txt");

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadIndexReadFileInPool,
                   index_path,
                   base::Passed(names.Pass()),
                   callback,
                   base::MessageLoopProxy::current()));
  }

  static void LoadIndexReadFileInPool(
      const base::FilePath& index_path,
      scoped_ptr<std::vector<std::string> > names,
      const StringsCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    std::string body;
    base::ReadFileToString(index_path, &body);

    original_loop->PostTask(FROM_HERE,
                            base::Bind(&SimpleCacheLoader::LoadIndexDidReadFile,
                                       base::Passed(names.Pass()),
                                       callback,
                                       body));
  }

  static void LoadIndexDidReadFile(scoped_ptr<std::vector<std::string> > names,
                                   const StringsCallback& callback,
                                   const std::string& serialized) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    ServiceWorkerCacheStorageIndex index;
    if (index.ParseFromString(serialized)) {
      for (int i = 0, max = index.cache_size(); i < max; ++i) {
        const ServiceWorkerCacheStorageIndex::Cache& cache = index.cache(i);
        names->push_back(cache.name());
      }
    }

    // TODO(jkarlin): Delete caches that are in the directory and not returned
    // in LoadIndex.
    callback.Run(names.Pass());
  }

 private:
  virtual ~SimpleCacheLoader() {}

  static std::string HexedHash(const std::string& value) {
    std::string value_hash = base::SHA1HashString(value);
    std::string valued_hexed_hash = base::StringToLowerASCII(
        base::HexEncode(value_hash.c_str(), value_hash.length()));
    return valued_hexed_hash;
  }

  static base::FilePath CreatePersistentCachePath(
      const base::FilePath& origin_path,
      const std::string& cache_name) {
    return origin_path.AppendASCII(HexedHash(cache_name));
  }

  const base::FilePath origin_path_;

  base::WeakPtrFactory<SimpleCacheLoader> weak_ptr_factory_;
};

ServiceWorkerCacheStorage::ServiceWorkerCacheStorage(
    const base::FilePath& path,
    bool memory_only,
    base::SequencedTaskRunner* cache_task_runner,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : initialized_(false),
      next_cache_id_(0),
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader(
        cache_task_runner_.get(), request_context, blob_context));
  else
    cache_loader_.reset(new SimpleCacheLoader(
        origin_path_, cache_task_runner_.get(), request_context, blob_context));
}

ServiceWorkerCacheStorage::~ServiceWorkerCacheStorage() {
  STLDeleteContainerPairSecondPointers(cache_map_.begin(), cache_map_.end());
}

void ServiceWorkerCacheStorage::CreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback) {
  if (!initialized_) {
    LazyInit(base::Bind(&ServiceWorkerCacheStorage::CreateCache,
                        weak_factory_.GetWeakPtr(),
                        cache_name,
                        callback));
    return;
  }

  if (GetLoadedCache(cache_name)) {
    callback.Run(kInvalidCacheID, CACHE_STORAGE_ERROR_EXISTS);
    return;
  }

  cache_loader_->CreateCache(
      cache_name,
      base::Bind(&ServiceWorkerCacheStorage::CreateCacheDidCreateCache,
                 weak_factory_.GetWeakPtr(),
                 cache_name,
                 callback));
}

void ServiceWorkerCacheStorage::GetCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_) {
    LazyInit(base::Bind(&ServiceWorkerCacheStorage::GetCache,
                        weak_factory_.GetWeakPtr(),
                        cache_name,
                        callback));
    return;
  }

  CacheContext* cache_context = GetLoadedCache(cache_name);
  if (!cache_context) {
    callback.Run(kInvalidCacheID, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  ServiceWorkerCache* cache = cache_context->cache.get();

  if (cache->HasCreatedBackend())
    return callback.Run(cache_context->id, CACHE_STORAGE_ERROR_NO_ERROR);

  cache->CreateBackend(base::Bind(&ServiceWorkerCacheStorage::DidCreateBackend,
                                  weak_factory_.GetWeakPtr(),
                                  cache->AsWeakPtr(),
                                  cache_context->id,
                                  callback));
}

void ServiceWorkerCacheStorage::HasCache(const std::string& cache_name,
                                         const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_) {
    LazyInit(base::Bind(&ServiceWorkerCacheStorage::HasCache,
                        weak_factory_.GetWeakPtr(),
                        cache_name,
                        callback));
    return;
  }

  bool has_cache = GetLoadedCache(cache_name) != NULL;

  callback.Run(has_cache, CACHE_STORAGE_ERROR_NO_ERROR);
}

void ServiceWorkerCacheStorage::DeleteCache(
    const std::string& cache_name,
    const BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_) {
    LazyInit(base::Bind(&ServiceWorkerCacheStorage::DeleteCache,
                        weak_factory_.GetWeakPtr(),
                        cache_name,
                        callback));
    return;
  }

  scoped_ptr<CacheContext> cache_context(GetLoadedCache(cache_name));
  if (!cache_context) {
    callback.Run(false, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  name_map_.erase(cache_name);
  cache_map_.erase(cache_context->id);
  cache_context.reset();

  // Update the Index
  cache_loader_->WriteIndex(
      cache_map_,
      base::Bind(&ServiceWorkerCacheStorage::DeleteCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(),
                 cache_name,
                 callback));
}

void ServiceWorkerCacheStorage::EnumerateCaches(
    const StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!initialized_) {
    LazyInit(base::Bind(&ServiceWorkerCacheStorage::EnumerateCaches,
                        weak_factory_.GetWeakPtr(),
                        callback));
    return;
  }

  std::vector<std::string> names;
  for (NameMap::const_iterator it = name_map_.begin(); it != name_map_.end();
       ++it) {
    names.push_back(it->first);
  }

  callback.Run(names, CACHE_STORAGE_ERROR_NO_ERROR);
}

void ServiceWorkerCacheStorage::DidCreateBackend(
    base::WeakPtr<ServiceWorkerCache> cache,
    CacheID cache_id,
    const CacheAndErrorCallback& callback,
    ServiceWorkerCache::ErrorType error) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (error != ServiceWorkerCache::ErrorTypeOK || !cache) {
    // TODO(jkarlin): This should delete the directory and try again in case
    // the cache is simply corrupt.
    callback.Run(kInvalidCacheID, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  callback.Run(cache_id, CACHE_STORAGE_ERROR_NO_ERROR);
}

// Init is run lazily so that it is called on the proper MessageLoop.
void ServiceWorkerCacheStorage::LazyInit(const base::Closure& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!initialized_);

  init_callbacks_.push_back(callback);

  // If this isn't the first call to LazyInit then return as the initialization
  // has already started.
  if (init_callbacks_.size() > 1u)
    return;

  // 1. Get the list of cache names (async call)
  // 2. For each cache name, load the cache (async call)
  // 3. Once each load is complete, update the map variables.
  // 4. Call the list of waiting callbacks.

  scoped_ptr<std::vector<std::string> > indexed_cache_names(
      new std::vector<std::string>());

  cache_loader_->LoadIndex(
      indexed_cache_names.Pass(),
      base::Bind(&ServiceWorkerCacheStorage::LazyInitDidLoadIndex,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerCacheStorage::LazyInitDidLoadIndex(
    const base::Closure& callback,
    scoped_ptr<std::vector<std::string> > indexed_cache_names) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (indexed_cache_names->empty()) {
    LazyInitDone();
    return;
  }

  std::vector<std::string>::const_iterator iter = indexed_cache_names->begin();
  std::vector<std::string>::const_iterator iter_next = iter + 1;

  cache_loader_->LoadCache(
      *iter,
      base::Bind(&ServiceWorkerCacheStorage::LazyInitIterateAndLoadCacheName,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Passed(indexed_cache_names.Pass()),
                 iter_next,
                 *iter));
}

void ServiceWorkerCacheStorage::LazyInitIterateAndLoadCacheName(
    const base::Closure& callback,
    scoped_ptr<std::vector<std::string> > indexed_cache_names,
    const std::vector<std::string>::const_iterator& iter,
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (cache)
    AddCacheToMaps(cache_name, cache.Pass());

  if (iter == indexed_cache_names->end()) {
    LazyInitDone();
    return;
  }

  std::vector<std::string>::const_iterator iter_next = iter + 1;
  cache_loader_->LoadCache(
      *iter,
      base::Bind(&ServiceWorkerCacheStorage::LazyInitIterateAndLoadCacheName,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 base::Passed(indexed_cache_names.Pass()),
                 iter_next,
                 *iter));
}

void ServiceWorkerCacheStorage::LazyInitDone() {
  initialized_ = true;
  for (std::vector<base::Closure>::iterator it = init_callbacks_.begin();
       it != init_callbacks_.end();
       ++it) {
    it->Run();
  }
  init_callbacks_.clear();
}

ServiceWorkerCacheStorage::CacheContext*
ServiceWorkerCacheStorage::AddCacheToMaps(
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheID id = next_cache_id_++;
  CacheContext* cache_context = new CacheContext(cache_name, id, cache.Pass());
  cache_map_.insert(std::make_pair(id, cache_context));  // Takes ownership
  name_map_.insert(std::make_pair(cache_name, id));
  return cache_context;
}

void ServiceWorkerCacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache) {
    callback.Run(kInvalidCacheID, CACHE_STORAGE_ERROR_CLOSING);
    return;
  }

  CacheContext* cache_context = AddCacheToMaps(cache_name, cache.Pass());

  cache_loader_->WriteIndex(
      cache_map_,
      base::Bind(&ServiceWorkerCacheStorage::CreateCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 cache_context->cache->AsWeakPtr(),
                 cache_context->id));
}

void ServiceWorkerCacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    base::WeakPtr<ServiceWorkerCache> cache,
    CacheID id,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!cache) {
    callback.Run(false, CACHE_STORAGE_ERROR_CLOSING);
    return;
  }

  cache->CreateBackend(base::Bind(&ServiceWorkerCacheStorage::DidCreateBackend,
                                  weak_factory_.GetWeakPtr(),
                                  cache,
                                  id,
                                  callback));
}

void ServiceWorkerCacheStorage::DeleteCacheDidWriteIndex(
    const std::string& cache_name,
    const BoolAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  cache_loader_->CleanUpDeletedCache(
      cache_name,
      base::Bind(&ServiceWorkerCacheStorage::DeleteCacheDidCleanUp,
                 weak_factory_.GetWeakPtr(),
                 callback));
}

void ServiceWorkerCacheStorage::DeleteCacheDidCleanUp(
    const BoolAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(true, CACHE_STORAGE_ERROR_NO_ERROR);
}

ServiceWorkerCacheStorage::CacheContext*
ServiceWorkerCacheStorage::GetLoadedCache(const std::string& cache_name) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  NameMap::const_iterator name_iter = name_map_.find(cache_name);
  if (name_iter == name_map_.end())
    return NULL;

  CacheMap::const_iterator map_iter = cache_map_.find(name_iter->second);
  DCHECK(map_iter != cache_map_.end());
  return map_iter->second;
}

}  // namespace content
