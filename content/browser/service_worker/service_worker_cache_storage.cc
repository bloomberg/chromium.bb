// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage.h"

#include <string>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/ref_counted.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"
#include "net/base/net_errors.h"
#include "webkit/browser/blob/blob_storage_context.h"

namespace content {

// Handles the loading and clean up of ServiceWorkerCache objects.
class ServiceWorkerCacheStorage::CacheLoader
    : public base::RefCountedThreadSafe<
          ServiceWorkerCacheStorage::CacheLoader> {
 public:
  typedef base::Callback<void(scoped_ptr<ServiceWorkerCache>)> CacheCallback;
  typedef base::Callback<void(bool)> BoolCallback;
  typedef base::Callback<void(scoped_ptr<std::vector<std::string> >)>
      StringsCallback;

  CacheLoader(
      base::SequencedTaskRunner* cache_task_runner,
      net::URLRequestContext* request_context,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_context)
      : cache_task_runner_(cache_task_runner),
        request_context_(request_context),
        blob_context_(blob_context) {}

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
  virtual void WriteIndex(CacheMap* caches, const BoolCallback& callback) = 0;

  // Loads the cache names from disk if applicable.
  virtual void LoadIndex(scoped_ptr<std::vector<std::string> > cache_names,
                         const StringsCallback& callback) = 0;

 protected:
  friend class base::RefCountedThreadSafe<
      ServiceWorkerCacheStorage::CacheLoader>;

  virtual ~CacheLoader() {};
  virtual void LoadCacheImpl(const std::string&) {}

  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;
  net::URLRequestContext* request_context_;
  base::WeakPtr<webkit_blob::BlobStorageContext> blob_context_;
};

class ServiceWorkerCacheStorage::MemoryLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  MemoryLoader(
      base::SequencedTaskRunner* cache_task_runner,
      net::URLRequestContext* request_context,
      base::WeakPtr<webkit_blob::BlobStorageContext> blob_context)
      : CacheLoader(cache_task_runner, request_context, blob_context) {}
  virtual void LoadCache(const std::string& cache_name,
                         const CacheCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  virtual void CreateCache(const std::string& cache_name,
                           const CacheCallback& callback) OVERRIDE {
    scoped_ptr<ServiceWorkerCache> cache =
        ServiceWorkerCache::CreateMemoryCache(
            cache_name, request_context_, blob_context_);
    callback.Run(cache.Pass());
  }

  virtual void CleanUpDeletedCache(const std::string& cache_name,
                                   const BoolCallback& callback) OVERRIDE {
    callback.Run(true);
  }

  virtual void WriteIndex(CacheMap* caches,
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
                    base::WeakPtr<webkit_blob::BlobStorageContext> blob_context)
      : CacheLoader(cache_task_runner, request_context, blob_context),
        origin_path_(origin_path) {}

  virtual void LoadCache(const std::string& cache_name,
                         const CacheCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the cache's directory if necessary. (LoadCreateDirectoryInPool)
    // 2. Create the cache object. (LoadDidCreateDirectory)

    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadCreateDirectoryInPool,
                   this,
                   CreatePersistentCachePath(origin_path_, cache_name),
                   cache_name,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  void LoadCreateDirectoryInPool(
      const base::FilePath& path,
      const std::string& cache_name,
      const CacheCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    DCHECK(cache_task_runner_->RunsTasksOnCurrentThread());

    bool rv = base::CreateDirectory(path);
    original_loop->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadDidCreateDirectory,
                   this,
                   cache_name,
                   callback,
                   rv));
  }

  void LoadDidCreateDirectory(const std::string& cache_name,
                              const CacheCallback& callback,
                              bool dir_rv) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    if (!dir_rv) {
      callback.Run(scoped_ptr<ServiceWorkerCache>());
      return;
    }

    scoped_ptr<ServiceWorkerCache> cache =
        ServiceWorkerCache::CreatePersistentCache(
            CreatePersistentCachePath(origin_path_, cache_name),
            cache_name,
            request_context_,
            blob_context_);
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
                   this,
                   cache_path,
                   cache_name,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  void CreateCacheDeleteFilesInPool(
      const base::FilePath& cache_path,
      const std::string& cache_name,
      const CacheCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    DCHECK(cache_task_runner_->RunsTasksOnCurrentThread());

    base::FilePath path(cache_path);
    if (base::PathExists(path))
      base::DeleteFile(path, /* recursive */ true);

    // Jump straight into LoadCache on the same thread.
    cache_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&SimpleCacheLoader::LoadCreateDirectoryInPool,
                   this,
                   cache_path,
                   cache_name,
                   callback,
                   original_loop));
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
                   this,
                   cache_path,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  void CleanUpDeleteCacheDirInPool(
      const base::FilePath& cache_path,
      const BoolCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    DCHECK(cache_task_runner_->RunsTasksOnCurrentThread());

    bool rv = base::DeleteFile(cache_path, true);
    original_loop->PostTask(FROM_HERE, base::Bind(callback, rv));
  }

  virtual void WriteIndex(CacheMap* caches,
                          const BoolCallback& callback) OVERRIDE {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    // 1. Create the index file as a string. (WriteIndex)
    // 2. Write the file to disk. (WriteIndexWriteToFileInPool)

    ServiceWorkerCacheStorageIndex index;

    for (CacheMap::const_iterator iter(caches); !iter.IsAtEnd();
         iter.Advance()) {
      const ServiceWorkerCache* cache = iter.GetCurrentValue();
      ServiceWorkerCacheStorageIndex::Cache* index_cache = index.add_cache();
      index_cache->set_name(cache->name());
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
                   this,
                   tmp_path,
                   index_path,
                   serialized,
                   caches,
                   callback,
                   base::MessageLoopProxy::current()));
  }

  void WriteIndexWriteToFileInPool(
      const base::FilePath& tmp_path,
      const base::FilePath& index_path,
      const std::string& data,
      CacheMap* caches,
      const BoolCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    DCHECK(cache_task_runner_->RunsTasksOnCurrentThread());

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
                   this,
                   index_path,
                   base::Passed(names.Pass()),
                   callback,
                   base::MessageLoopProxy::current()));
  }

  void LoadIndexReadFileInPool(
      const base::FilePath& index_path,
      scoped_ptr<std::vector<std::string> > names,
      const StringsCallback& callback,
      const scoped_refptr<base::MessageLoopProxy>& original_loop) {
    DCHECK(cache_task_runner_->RunsTasksOnCurrentThread());

    std::string body;
    base::ReadFileToString(index_path, &body);

    original_loop->PostTask(FROM_HERE,
                            base::Bind(&SimpleCacheLoader::LoadIndexDidReadFile,
                                       this,
                                       base::Passed(names.Pass()),
                                       callback,
                                       body));
  }

  void LoadIndexDidReadFile(scoped_ptr<std::vector<std::string> > names,
                            const StringsCallback& callback,
                            const std::string& serialized) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);

    ServiceWorkerCacheStorageIndex index;
    index.ParseFromString(serialized);

    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const ServiceWorkerCacheStorageIndex::Cache& cache = index.cache(i);
      names->push_back(cache.name());
    }

    // TODO(jkarlin): Delete caches that are in the directory and not returned
    // in LoadIndex.
    callback.Run(names.Pass());
  }

 private:
  virtual ~SimpleCacheLoader() {}

  std::string HexedHash(const std::string& value) {
    std::string value_hash = base::SHA1HashString(value);
    std::string valued_hexed_hash = base::StringToLowerASCII(
        base::HexEncode(value_hash.c_str(), value_hash.length()));
    return valued_hexed_hash;
  }

  base::FilePath CreatePersistentCachePath(const base::FilePath& origin_path,
                                           const std::string& cache_name) {
    return origin_path.AppendASCII(HexedHash(cache_name));
  }

  const base::FilePath origin_path_;
};

ServiceWorkerCacheStorage::ServiceWorkerCacheStorage(
    const base::FilePath& path,
    bool memory_only,
    base::SequencedTaskRunner* cache_task_runner,
    net::URLRequestContext* request_context,
    base::WeakPtr<webkit_blob::BlobStorageContext> blob_context)
    : initialized_(false),
      origin_path_(path),
      cache_task_runner_(cache_task_runner),
      weak_factory_(this) {
  if (memory_only)
    cache_loader_ =
        new MemoryLoader(cache_task_runner_, request_context, blob_context);
  else
    cache_loader_ = new SimpleCacheLoader(
        origin_path_, cache_task_runner_, request_context, blob_context);
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

  if (cache_name.empty()) {
    callback.Run(0, CACHE_STORAGE_ERROR_EMPTY_KEY);
    return;
  }

  if (GetLoadedCache(cache_name)) {
    callback.Run(0, CACHE_STORAGE_ERROR_EXISTS);
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

  if (cache_name.empty()) {
    callback.Run(0, CACHE_STORAGE_ERROR_EMPTY_KEY);
    return;
  }

  ServiceWorkerCache* cache = GetLoadedCache(cache_name);
  if (!cache) {
    callback.Run(0, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  cache->CreateBackend(base::Bind(&ServiceWorkerCacheStorage::DidCreateBackend,
                                  weak_factory_.GetWeakPtr(),
                                  cache->AsWeakPtr(),
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

  if (cache_name.empty()) {
    callback.Run(false, CACHE_STORAGE_ERROR_EMPTY_KEY);
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

  if (cache_name.empty()) {
    callback.Run(false, CACHE_STORAGE_ERROR_EMPTY_KEY);
    return;
  }

  ServiceWorkerCache* cache = GetLoadedCache(cache_name);
  if (!cache) {
    callback.Run(false, CACHE_STORAGE_ERROR_NOT_FOUND);
    return;
  }

  name_map_.erase(cache_name);
  cache_map_.Remove(cache->id());  // deletes cache

  // Update the Index
  cache_loader_->WriteIndex(
      &cache_map_,
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
    const CacheAndErrorCallback& callback,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!success || !cache) {
    // TODO(jkarlin): This should delete the directory and try again in case
    // the cache is simply corrupt.
    callback.Run(0, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }
  callback.Run(cache->id(), CACHE_STORAGE_ERROR_NO_ERROR);
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
                 iter_next));
}

void ServiceWorkerCacheStorage::LazyInitIterateAndLoadCacheName(
    const base::Closure& callback,
    scoped_ptr<std::vector<std::string> > indexed_cache_names,
    const std::vector<std::string>::const_iterator& iter,
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (cache)
    AddCacheToMaps(cache.Pass());

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
                 iter_next));
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

void ServiceWorkerCacheStorage::AddCacheToMaps(
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCache* cache_ptr = cache.release();
  CacheID id = cache_map_.Add(cache_ptr);
  name_map_.insert(std::make_pair(cache_ptr->name(), id));
  cache_ptr->set_id(id);
}

void ServiceWorkerCacheStorage::CreateCacheDidCreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback,
    scoped_ptr<ServiceWorkerCache> cache) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache) {
    callback.Run(0, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }

  base::WeakPtr<ServiceWorkerCache> cache_ptr = cache->AsWeakPtr();

  AddCacheToMaps(cache.Pass());

  cache_loader_->WriteIndex(
      &cache_map_,
      base::Bind(
          &ServiceWorkerCacheStorage::CreateCacheDidWriteIndex,
          weak_factory_.GetWeakPtr(),
          callback,
          cache_ptr));  // cache is owned by this->CacheMap and won't be deleted
}

void ServiceWorkerCacheStorage::CreateCacheDidWriteIndex(
    const CacheAndErrorCallback& callback,
    base::WeakPtr<ServiceWorkerCache> cache,
    bool success) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (!cache) {
    callback.Run(false, CACHE_STORAGE_ERROR_STORAGE);
    return;
  }
  cache->CreateBackend(base::Bind(&ServiceWorkerCacheStorage::DidCreateBackend,
                                  weak_factory_.GetWeakPtr(),
                                  cache,
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

ServiceWorkerCache* ServiceWorkerCacheStorage::GetLoadedCache(
    const std::string& cache_name) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(initialized_);

  NameMap::const_iterator it = name_map_.find(cache_name);
  if (it == name_map_.end())
    return NULL;

  ServiceWorkerCache* cache = cache_map_.Lookup(it->second);
  DCHECK(cache);
  return cache;
}

}  // namespace content
