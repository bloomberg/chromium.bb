// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/threading/thread_checker.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ServiceWorkerCache;

// TODO(jkarlin): Constrain the total bytes used per origin.
// The set of caches for a given origin. It is owned by the
// ServiceWorkerCacheStorageManager. Provided callbacks are called on the
// |callback_loop| provided in the constructor.
class ServiceWorkerCacheStorage {
 public:
  enum CacheStorageError {
    CACHE_STORAGE_ERROR_NO_ERROR,
    CACHE_STORAGE_ERROR_NOT_IMPLEMENTED,
    CACHE_STORAGE_ERROR_NOT_FOUND,
    CACHE_STORAGE_ERROR_EXISTS,
    CACHE_STORAGE_ERROR_STORAGE,
    CACHE_STORAGE_ERROR_EMPTY_KEY,
  };

  typedef base::Callback<void(bool, CacheStorageError)> BoolAndErrorCallback;
  typedef base::Callback<void(int, CacheStorageError)> CacheAndErrorCallback;
  typedef base::Callback<void(const std::vector<std::string>&,
                              CacheStorageError)> StringsAndErrorCallback;

  ServiceWorkerCacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      const scoped_refptr<base::MessageLoopProxy>& callback_loop);

  virtual ~ServiceWorkerCacheStorage();

  // Create a ServiceWorkerCache if it doesn't already exist and call the
  // callback with the cache's id. If it already
  // exists the callback is called with CACHE_STORAGE_ERROR_EXISTS.
  void CreateCache(const std::string& cache_name,
                   const CacheAndErrorCallback& callback);

  // Get the cache id for the given key. If not found returns
  // CACHE_STORAGE_ERROR_NOT_FOUND.
  void GetCache(const std::string& cache_name,
                const CacheAndErrorCallback& callback);

  // Calls the callback with whether or not the cache exists.
  void HasCache(const std::string& cache_name,
                const BoolAndErrorCallback& callback);

  // Deletes the cache if it exists. If it doesn't exist,
  // CACHE_STORAGE_ERROR_NOT_FOUND is returned.
  void DeleteCache(const std::string& cache_name,
                   const BoolAndErrorCallback& callback);

  // Calls the callback with a vector of cache names (keys) available.
  void EnumerateCaches(const StringsAndErrorCallback& callback);

  // TODO(jkarlin): Add match() function.

  void InitializeCacheCallback(const ServiceWorkerCache* cache,
                               const CacheAndErrorCallback& callback,
                               bool success);

 private:
  class MemoryLoader;
  class SimpleCacheLoader;
  class CacheLoader;

  typedef IDMap<ServiceWorkerCache, IDMapOwnPointer> CacheMap;
  typedef CacheMap::KeyType CacheID;
  typedef std::map<std::string, CacheID> NameMap;

  ServiceWorkerCache* GetLoadedCache(const std::string& cache_name) const;

  void LazyInit();
  void InitCache(ServiceWorkerCache* cache);

  bool initialized_;
  CacheMap cache_map_;
  NameMap name_map_;
  base::FilePath origin_path_;
  scoped_refptr<base::MessageLoopProxy> callback_loop_;
  scoped_ptr<CacheLoader> cache_loader_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_
