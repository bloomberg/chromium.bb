// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage.h"

#include <string>

#include "base/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/directory_lister.h"

namespace content {

// Handles the loading and clean up of ServiceWorkerCache objects.
class ServiceWorkerCacheStorage::CacheLoader {
 public:
  virtual ~CacheLoader() {};
  virtual ServiceWorkerCache* LoadCache(const std::string& cache_name) = 0;
  // Creates a new cache, deleting any pre-existing cache of the same name.
  virtual ServiceWorkerCache* CreateCache(const std::string& cache_name) = 0;
  virtual bool CleanUpDeletedCache(const std::string& key) = 0;
  virtual bool WriteIndex(CacheMap* caches) = 0;
  virtual void LoadIndex(std::vector<std::string>* cache_names) = 0;
};

class ServiceWorkerCacheStorage::MemoryLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  virtual content::ServiceWorkerCache* LoadCache(
      const std::string& cache_name) OVERRIDE {
    NOTREACHED();
    return NULL;
  }

  virtual ServiceWorkerCache* CreateCache(
      const std::string& cache_name) OVERRIDE {
    return ServiceWorkerCache::CreateMemoryCache(cache_name);
  }

  virtual bool CleanUpDeletedCache(const std::string& cache_name) OVERRIDE {
    return true;
  }

  virtual bool WriteIndex(CacheMap* caches) OVERRIDE { return false; }

  virtual void LoadIndex(std::vector<std::string>* cache_names) OVERRIDE {
    return;
  }
};

class ServiceWorkerCacheStorage::SimpleCacheLoader
    : public ServiceWorkerCacheStorage::CacheLoader {
 public:
  explicit SimpleCacheLoader(const base::FilePath& origin_path)
      : origin_path_(origin_path) {}

  virtual ServiceWorkerCache* LoadCache(
      const std::string& cache_name) OVERRIDE {
    base::CreateDirectory(CreatePersistentCachePath(origin_path_, cache_name));
    return ServiceWorkerCache::CreatePersistentCache(
        CreatePersistentCachePath(origin_path_, cache_name), cache_name);
  }

  virtual ServiceWorkerCache* CreateCache(
      const std::string& cache_name) OVERRIDE {
    base::FilePath cache_path =
        CreatePersistentCachePath(origin_path_, cache_name);
    if (base::PathExists(cache_path))
      base::DeleteFile(cache_path, /* recursive */ true);
    return LoadCache(cache_name);
  }

  virtual bool CleanUpDeletedCache(const std::string& cache_name) OVERRIDE {
    return base::DeleteFile(CreatePersistentCachePath(origin_path_, cache_name),
                            true);
  }

  virtual bool WriteIndex(CacheMap* caches) OVERRIDE {
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

    int bytes_written =
        base::WriteFile(tmp_path, serialized.c_str(), serialized.size());
    if (bytes_written != implicit_cast<int>(serialized.size())) {
      base::DeleteFile(tmp_path, /* recursive */ false);
      return false;
    }

    // Atomically rename the temporary index file to become the real one.
    return base::ReplaceFile(tmp_path, index_path, NULL);
  }

  virtual void LoadIndex(std::vector<std::string>* names) OVERRIDE {
    base::FilePath index_path = origin_path_.AppendASCII("index.txt");

    std::string serialized;
    if (!base::ReadFileToString(index_path, &serialized))
      return;

    ServiceWorkerCacheStorageIndex index;
    index.ParseFromString(serialized);

    for (int i = 0, max = index.cache_size(); i < max; ++i) {
      const ServiceWorkerCacheStorageIndex::Cache& cache = index.cache(i);
      names->push_back(cache.name());
    }

    // TODO(jkarlin): Delete caches that are in the directory and not returned
    // in LoadIndex.
    return;
  }

 private:
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
    const scoped_refptr<base::MessageLoopProxy>& callback_loop)
    : initialized_(false), origin_path_(path), callback_loop_(callback_loop) {
  if (memory_only)
    cache_loader_.reset(new MemoryLoader());
  else
    cache_loader_.reset(new SimpleCacheLoader(origin_path_));
}

ServiceWorkerCacheStorage::~ServiceWorkerCacheStorage() {
}

void ServiceWorkerCacheStorage::CreateCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback) {
  LazyInit();

  if (cache_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_EMPTY_KEY));
    return;
  }

  if (GetLoadedCache(cache_name)) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_EXISTS));
    return;
  }

  ServiceWorkerCache* cache = cache_loader_->CreateCache(cache_name);

  if (!cache) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_STORAGE));
    return;
  }

  InitCache(cache);

  cache_loader_->WriteIndex(&cache_map_);

  cache->CreateBackend(
      base::Bind(&ServiceWorkerCacheStorage::InitializeCacheCallback,
                 base::Unretained(this),
                 cache,
                 callback));
}

void ServiceWorkerCacheStorage::GetCache(
    const std::string& cache_name,
    const CacheAndErrorCallback& callback) {
  LazyInit();

  if (cache_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_EMPTY_KEY));
    return;
  }

  ServiceWorkerCache* cache = GetLoadedCache(cache_name);
  if (!cache) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_NOT_FOUND));
    return;
  }

  cache->CreateBackend(
      base::Bind(&ServiceWorkerCacheStorage::InitializeCacheCallback,
                 base::Unretained(this),
                 cache,
                 callback));
}

void ServiceWorkerCacheStorage::HasCache(const std::string& cache_name,
                                         const BoolAndErrorCallback& callback) {
  LazyInit();

  if (cache_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, CACHE_STORAGE_ERROR_EMPTY_KEY));
    return;
  }

  bool has_cache = GetLoadedCache(cache_name) != NULL;

  callback_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          callback, has_cache, CACHE_STORAGE_ERROR_NO_ERROR));
}

void ServiceWorkerCacheStorage::DeleteCache(
    const std::string& cache_name,
    const BoolAndErrorCallback& callback) {
  LazyInit();

  if (cache_name.empty()) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, CACHE_STORAGE_ERROR_EMPTY_KEY));
    return;
  }

  ServiceWorkerCache* cache = GetLoadedCache(cache_name);
  if (!cache) {
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, false, CACHE_STORAGE_ERROR_NOT_FOUND));
    return;
  }

  name_map_.erase(cache_name);
  cache_map_.Remove(cache->id());  // deletes cache

  cache_loader_->WriteIndex(&cache_map_);  // Update the index.

  cache_loader_->CleanUpDeletedCache(cache_name);

  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, true, CACHE_STORAGE_ERROR_NO_ERROR));
}

void ServiceWorkerCacheStorage::EnumerateCaches(
    const StringsAndErrorCallback& callback) {
  LazyInit();

  std::vector<std::string> names;
  for (NameMap::const_iterator it = name_map_.begin(); it != name_map_.end();
       ++it) {
    names.push_back(it->first);
  }

  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, names, CACHE_STORAGE_ERROR_NO_ERROR));
}

void ServiceWorkerCacheStorage::InitializeCacheCallback(
    const ServiceWorkerCache* cache,
    const CacheAndErrorCallback& callback,
    bool success) {
  if (!success) {
    // TODO(jkarlin): This should delete the directory and try again in case
    // the cache is simply corrupt.
    callback_loop_->PostTask(
        FROM_HERE, base::Bind(callback, 0, CACHE_STORAGE_ERROR_STORAGE));
    return;
  }
  callback_loop_->PostTask(
      FROM_HERE,
      base::Bind(callback, cache->id(), CACHE_STORAGE_ERROR_NO_ERROR));
}

// Init is run lazily so that it is called on the proper MessageLoop.
void ServiceWorkerCacheStorage::LazyInit() {
  if (initialized_)
    return;

  std::vector<std::string> indexed_cache_names;
  cache_loader_->LoadIndex(&indexed_cache_names);

  for (std::vector<std::string>::const_iterator it =
           indexed_cache_names.begin();
       it != indexed_cache_names.end();
       ++it) {
    ServiceWorkerCache* cache = cache_loader_->LoadCache(*it);
    InitCache(cache);
  }
  initialized_ = true;
}

void ServiceWorkerCacheStorage::InitCache(ServiceWorkerCache* cache) {
  CacheID id = cache_map_.Add(cache);
  name_map_.insert(std::make_pair(cache->name(), id));
  cache->set_id(id);
}

ServiceWorkerCache* ServiceWorkerCacheStorage::GetLoadedCache(
    const std::string& cache_name) const {
  DCHECK(initialized_);

  NameMap::const_iterator it = name_map_.find(cache_name);
  if (it == name_map_.end())
    return NULL;

  ServiceWorkerCache* cache = cache_map_.Lookup(it->second);
  DCHECK(cache);
  return cache;
}

}  // namespace content
