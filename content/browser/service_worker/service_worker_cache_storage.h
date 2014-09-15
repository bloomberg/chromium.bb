// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_cache.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContext;
}

namespace storage {
class BlobStorageContext;
}

namespace content {

// TODO(jkarlin): Constrain the total bytes used per origin.

// ServiceWorkerCacheStorage holds the set of caches for a given origin. It is
// owned by the ServiceWorkerCacheStorageManager. This class expects to be run
// on the IO thread.
class CONTENT_EXPORT ServiceWorkerCacheStorage {
 public:
  enum CacheStorageError {
    CACHE_STORAGE_ERROR_NO_ERROR,
    CACHE_STORAGE_ERROR_NOT_IMPLEMENTED,
    CACHE_STORAGE_ERROR_NOT_FOUND,
    CACHE_STORAGE_ERROR_EXISTS,
    CACHE_STORAGE_ERROR_STORAGE,
    CACHE_STORAGE_ERROR_CLOSING
  };
  typedef std::vector<std::string> StringVector;
  typedef base::Callback<void(bool, CacheStorageError)> BoolAndErrorCallback;
  typedef base::Callback<void(const scoped_refptr<ServiceWorkerCache>&,
                              CacheStorageError)> CacheAndErrorCallback;
  typedef base::Callback<void(const StringVector&, CacheStorageError)>
      StringsAndErrorCallback;

  ServiceWorkerCacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      net::URLRequestContext* request_context,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

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

 private:
  class MemoryLoader;
  class SimpleCacheLoader;
  class CacheLoader;

  typedef std::map<std::string, base::WeakPtr<ServiceWorkerCache> > CacheMap;

  // Return a ServiceWorkerCache for the given name if the name is known. If the
  // ServiceWorkerCache has been deleted, creates a new one.
  scoped_refptr<ServiceWorkerCache> GetLoadedCache(
      const std::string& cache_name);

  // Initializer and its callback are below. While LazyInit is running any new
  // operations will be queued and started in order after initialization.
  void LazyInit(const base::Closure& closure);
  void LazyInitDidLoadIndex(
      const base::Closure& callback,
      scoped_ptr<std::vector<std::string> > indexed_cache_names);

  void AddCacheToMap(const std::string& cache_name,
                     base::WeakPtr<ServiceWorkerCache> cache);

  // The CreateCache callbacks are below.
  void CreateCacheDidCreateCache(
      const std::string& cache_name,
      const CacheAndErrorCallback& callback,
      const scoped_refptr<ServiceWorkerCache>& cache);
  void CreateCacheDidWriteIndex(const CacheAndErrorCallback& callback,
                                const scoped_refptr<ServiceWorkerCache>& cache,
                                bool success);

  // The DeleteCache callbacks are below.
  void DeleteCacheDidWriteIndex(const std::string& cache_name,
                                const BoolAndErrorCallback& callback,
                                bool success);
  void DeleteCacheDidCleanUp(const BoolAndErrorCallback& callback,
                             bool success);

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_;

  // The list of operations waiting on initialization.
  std::vector<base::Closure> init_callbacks_;

  // The map of cache names to ServiceWorkerCache objects.
  CacheMap cache_map_;

  // The names of caches in the order that they were created.
  StringVector ordered_cache_names_;

  // The file path for this CacheStorage.
  base::FilePath origin_path_;

  // The TaskRunner to run file IO on.
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // Whether or not to store data in disk or memory.
  bool memory_only_;

  // Performs backend specific operations (memory vs disk).
  scoped_ptr<CacheLoader> cache_loader_;

  base::WeakPtrFactory<ServiceWorkerCacheStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_H_
