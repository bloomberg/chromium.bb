// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage.h"

#include <string>

#include "base/files/file_util.h"
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
#include "storage/browser/blob/blob_storage_context.h"

namespace content {


// Handles the loading and clean up of ServiceWorkerCache objects. The
// callback of every public method is guaranteed to be called.
class ServiceWorkerCacheStorage::CacheLoader {
 public:
  typedef base::Callback<void(const scoped_refptr<ServiceWorkerCache>&)>
      CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(scoped_ptr<std::vector<std::string> >)>
      StringVectorCallback;

  CacheLoader(base::SequencedTaskRunner* cache_task_runner,
              net::URLRequestContext* request_context,
              base::WeakPtr<storage::BlobStorageContext> blob_context)
      : cache_task_runner_(cache_task_runner),
        request_context_(request_context),
        blob_context_(blob_context) {}

  virtual ~CacheLoader() {}

  // Creates a ServiceWorkerCache with the given name. It does not attempt to
  // load the backend, that happens lazily when the cache is used.
  virtual scoped_refptr<ServiceWorkerCache> CreateServiceWorkerCache(
      const std::string& cache_name) = 0;

  // Deletes any pre-existing cache of the same name and then loads it.
  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) = 0;

  // After the backend has been deleted, do any extra house keeping such as
  // removing the cache's directory.
  virtual void CleanUpDeletedCache(const std::string& key,
                                   const BoolCallback& callback) = 0;

  // Writes the cache names (and sizes) to disk if applicable.
  virtual void WriteIndex(const StringVector& cache_names,
                          const BoolCallback& callback) = 0;

  // Loads the cache names from disk if applicable.
  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > cache_names,
                         const StringVectorCallback& callback) = 0;

 protected:
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  net::URLRequestContext* request_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_context_;
};

// Creates memory-only ServiceWorkerCaches. Because these caches have no
// persistent storage it is not safe to free them from memory if they might be
// used again. Therefore this class holds a reference to each cache until the
// cache is deleted.
class ServiceWorkerCacheStorage::MemoryLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  MemoryLoader(base::SequencedTaskRunner* cache_task_runner,
               net::URLRequestContext* request_context,
               base::WeakPtr<storage::BlobStorageContext> blob_context)
      : CacheLoader(cache_task_runner, request_context, blob_context) {}

  virtual scoped_refptr<ServiceWorkerCache> CreateServiceWorkerCache(
      const std::string& cache_name) OVERRIDE {
    return ServiceWorkerCache::CreateMemoryCache(request_context_,
                                                 blob_context_);
  }

  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) OVERRIDE {
    scoped_refptr<ServiceWorkerCache> cache =
        ServiceWorkerCache::CreateMemoryCache(request_context_, blob_context_);
    cache_refs_.insert(std::make_pair(cache_name, cache));
    callback.Run(cache);
  }

  virtual void CleanUpDeletedCache(const std::string& cache_name,
                                   const BoolCallback& callback) OVERRIDE {
    CacheRefMap::iterator it = cache_refs_.find(cache_name);
    DCHECK(it != cache_refs_.end());
    cache_refs_.erase(it);
    callback.Run(true);
  }

  virtual void WriteIndex(const StringVector& cache_names,
                          const BoolCallback& callback) OVERRIDE {
    callback.Run(false);
  }

  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > cache_names,
                         const StringVectorCallback& callback) OVERRIDE {
    callback.Run(cache_names.Pass());
  }

 private:
  typedef std::map<std::string, scoped_refptr<ServiceWorkerCache> > CacheRefMap;
  virtual ~MemoryLoader() {}

  // Keep a reference to each cache to ensure that it's not freed before the
  // client calls ServiceWorkerCacheStorage::Delete or the CacheStorage is
  // freed.
  CacheRefMap cache_refs_;
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

  virtual scoped_refptr<ServiceWorkerCache> CreateServiceWorkerCache(
      const std::string& cache_name) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    return ServiceWorkerCache::CreatePersistentCache(
        CreatePersistentCachePath(origin_path_, cache_name),
        request_context_,
        blob_context_);
  }

  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Delete the cache's directory if it exists.
    // (CreateCacheDeleteFilesInPool)
    // 2. Load the cache. (LoadCreateDirectoryInPool)

    base::FilePath cache_path =
        CreatePersistentCachePath(origin_path_, cache_name);

    PostTaskAndReplyWithResult(
        cache_task_runner_.get(),
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::CreateCachePrepDirInPool, cache_path),
        base::Bind(&SimpleCacheLoader::CreateCachePreppedDir,
                   cache_name,
                   callback,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  static bool CreateCachePrepDirInPool(const base::FilePath& cache_path) {
    if (base::PathExists(cache_path))
      base::DeleteFile(cache_path, /* recursive */ true);
    return base::CreateDirectory(cache_path);
  }

  static void CreateCachePreppedDir(const std::string& cache_name,
                                    const CacheCallback& callback,
                                    base::WeakPtr<SimpleCacheLoader> loader,
                                    bool success) {
    if (!success || !loader) {
      callback.Run(scoped_refptr<ServiceWorkerCache>());
      return;
    }

    callback.Run(loader->CreateServiceWorkerCache(cache_name));
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

  virtual void WriteIndex(const StringVector& cache_names,
                          const BoolCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    ServiceWorkerCacheStorageIndex index;

    for (size_t i = 0u, max = cache_names.size(); i < max; ++i) {
      ServiceWorkerCacheStorageIndex::Cache* index_cache = index.add_cache();
      index_cache->set_name(cache_names[i]);
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
                         const StringVectorCallback& callback) OVERRIDE {
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
      const StringVectorCallback& callback,
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
                                   const StringVectorCallback& callback,
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
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      memory_only_(memory_only),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader(
        cache_task_runner_.get(), request_context, blob_context));
  else
    cache_loader_.reset(new SimpleCacheLoader(
        origin_path_, cache_task_runner_.get(), request_context, blob_context));
}

ServiceWorkerCacheStorage::~ServiceWorkerCacheStorage() {
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

  if (cache_map_.find(cache_name) != cache_map_.end()) {
    callback.Run(scoped_refptr<ServiceWorkerCache>(),
                 CACHE_STORAGE_ERROR_EXISTS);
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

  scoped_refptr<ServiceWorkerCache> cache = GetLoadedCache(cache_name);
  if (!cache.get()) {
    callback.Run(scoped_refptr<ServiceWorkerCache>(),
                 CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  callback.Run(cache, CACHE_STORAGE_ERROR_NO_ERROR);
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

  bool has_cache = cache_map_.find(cache_name) != cache_map_.end();

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

  CacheMap::iterator it = cache_map_.find(cache_name);
  if (it == cache_map_.end()) {
    callback.Run(false, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  base::WeakPtr<ServiceWorkerCache> cache = it->second;
  if (cache)
    cache->Close();

  cache_map_.erase(it);

  // Delete the name from ordered_cache_names_.
  StringVector::iterator iter = std::find(
      ordered_cache_names_.begin(), ordered_cache_names_.end(), cache_name);
  DCHECK(iter != ordered_cache_names_.end());
  ordered_cache_names_.erase(iter);

  // Update the Index
  cache_loader_->WriteIndex(
      ordered_cache_names_,
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

  callback.Run(ordered_cache_names_, CACHE_STORAGE_ERROR_NO_ERROR);
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

  for (size_t i = 0u, max = indexed_cache_names->size(); i < max; ++i) {
    cache_map_.insert(std::make_pair(indexed_cache_names->at(i),
                                     base::WeakPtr<ServiceWorkerCache>()));
    ordered_cache_names_.push_back(indexed_cache_names->at(i));
  }

  initialized_ = true;
  for (std::vector<base::Closure>::iterator it = init_callbacks_.begin();
       it != init_callbacks_.end();
       ++it) {
    it->Run();
  }
  init_callbacks_.clear();
}

void ServiceWorkerCacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    const scoped_refptr<ServiceWorkerCache>& cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache.get()) {
    callback.Run(scoped_refptr<ServiceWorkerCache>(),
                 CACHE_STORAGE_ERROR_CLOSING);
    return;
  }

  cache_map_.insert(std::make_pair(cache_name, cache->AsWeakPtr()));
  ordered_cache_names_.push_back(cache_name);

  cache_loader_->WriteIndex(
      ordered_cache_names_,
      base::Bind(&ServiceWorkerCacheStorage::CreateCacheDidWriteIndex,
                 weak_factory_.GetWeakPtr(),
                 callback,
                 cache));
}

void ServiceWorkerCacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    const scoped_refptr<ServiceWorkerCache>& cache,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache.get());

  callback.Run(cache, CACHE_STORAGE_ERROR_NO_ERROR);
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

scoped_refptr<ServiceWorkerCache> ServiceWorkerCacheStorage::GetLoadedCache(
    const std::string& cache_name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  CacheMap::iterator map_iter = cache_map_.find(cache_name);
  if (map_iter == cache_map_.end())
    return scoped_refptr<ServiceWorkerCache>();

  base::WeakPtr<ServiceWorkerCache> cache = map_iter->second;

  if (!cache) {
    scoped_refptr<ServiceWorkerCache> new_cache =
        cache_loader_->CreateServiceWorkerCache(cache_name);
    map_iter->second = new_cache->AsWeakPtr();
    return new_cache;
  }

  return make_scoped_refptr(cache.get());
}

}  // namespace content
