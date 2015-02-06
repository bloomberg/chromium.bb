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
class ServiceWorkerCacheScheduler;

// TODO(jkarlin): Constrain the total bytes used per origin.

// ServiceWorkerCacheStorage holds the set of caches for a given origin. It is
// owned by the ServiceWorkerCacheStorageManager. This class expects to be run
// on the IO thread. The asynchronous methods are executed serially.
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

  static const char kIndexFileName[];

  ServiceWorkerCacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin);

  // Any unfinished asynchronous operations may not complete or call their
  // callbacks.
  virtual ~ServiceWorkerCacheStorage();

  // Get the cache for the given key. If the cache is not found it is
  // created.
  void OpenCache(const std::string& cache_name,
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

  // Calls match on the cache with the given |cache_name|.
  void MatchCache(const std::string& cache_name,
                  scoped_ptr<ServiceWorkerFetchRequest> request,
                  const ServiceWorkerCache::ResponseCallback& callback);

  // Calls match on all of the caches in parallel, calling |callback| with the
  // first response found. Note that if multiple caches have the same
  // request/response then it is not defined which cache's response will be
  // returned. If no response is found then |callback| is called with
  // ServiceWorkerCache::ErrorTypeNotFound.
  void MatchAllCaches(scoped_ptr<ServiceWorkerFetchRequest> request,
                      const ServiceWorkerCache::ResponseCallback& callback);

  // Calls close on each cache and runs the callback after all of them have
  // closed.
  void CloseAllCaches(const base::Closure& callback);

  // The size of all of the origin's contents in memory. Returns 0 if the cache
  // backend is not a memory backend. Runs synchronously.
  int64 MemoryBackedSize() const;

  // The functions below are for tests to verify that the operations run
  // serially.
  void StartAsyncOperationForTesting();
  void CompleteAsyncOperationForTesting();

 private:
  class MemoryLoader;
  class SimpleCacheLoader;
  class CacheLoader;

  typedef std::map<std::string, base::WeakPtr<ServiceWorkerCache> > CacheMap;

  // Return a ServiceWorkerCache for the given name if the name is known. If the
  // ServiceWorkerCache has been deleted, creates a new one.
  scoped_refptr<ServiceWorkerCache> GetLoadedCache(
      const std::string& cache_name);

  // Initializer and its callback are below.
  void LazyInit();
  void LazyInitImpl();
  void LazyInitDidLoadIndex(
      scoped_ptr<std::vector<std::string> > indexed_cache_names);

  // The Open and CreateCache callbacks are below.
  void OpenCacheImpl(const std::string& cache_name,
                     const CacheAndErrorCallback& callback);
  void CreateCacheDidCreateCache(
      const std::string& cache_name,
      const CacheAndErrorCallback& callback,
      const scoped_refptr<ServiceWorkerCache>& cache);
  void CreateCacheDidWriteIndex(const CacheAndErrorCallback& callback,
                                const scoped_refptr<ServiceWorkerCache>& cache,
                                bool success);

  // The HasCache callbacks are below.
  void HasCacheImpl(const std::string& cache_name,
                    const BoolAndErrorCallback& callback);

  // The DeleteCache callbacks are below.
  void DeleteCacheImpl(const std::string& cache_name,
                       const BoolAndErrorCallback& callback);

  void DeleteCacheDidClose(const std::string& cache_name,
                           const BoolAndErrorCallback& callback,
                           const StringVector& ordered_cache_names,
                           const scoped_refptr<ServiceWorkerCache>& cache);
  void DeleteCacheDidWriteIndex(const std::string& cache_name,
                                const BoolAndErrorCallback& callback,
                                bool success);
  void DeleteCacheDidCleanUp(const BoolAndErrorCallback& callback,
                             bool success);

  // The EnumerateCache callbacks are below.
  void EnumerateCachesImpl(const StringsAndErrorCallback& callback);

  // The MatchCache callbacks are below.
  void MatchCacheImpl(const std::string& cache_name,
                      scoped_ptr<ServiceWorkerFetchRequest> request,
                      const ServiceWorkerCache::ResponseCallback& callback);
  void MatchCacheDidMatch(const scoped_refptr<ServiceWorkerCache>& cache,
                          const ServiceWorkerCache::ResponseCallback& callback,
                          ServiceWorkerCache::ErrorType error,
                          scoped_ptr<ServiceWorkerResponse> response,
                          scoped_ptr<storage::BlobDataHandle> handle);

  // The MatchAllCaches callbacks are below.
  void MatchAllCachesImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                          const ServiceWorkerCache::ResponseCallback& callback);
  void MatchAllCachesDidMatch(scoped_refptr<ServiceWorkerCache> cache,
                              const base::Closure& barrier_closure,
                              ServiceWorkerCache::ResponseCallback* callback,
                              ServiceWorkerCache::ErrorType error,
                              scoped_ptr<ServiceWorkerResponse> response,
                              scoped_ptr<storage::BlobDataHandle> handle);
  void MatchAllCachesDidMatchAll(
      scoped_ptr<ServiceWorkerCache::ResponseCallback> callback);

  // The CloseAllCaches callbacks are below.
  void CloseAllCachesImpl(const base::Closure& callback);

  void PendingClosure(const base::Closure& callback);
  void PendingBoolAndErrorCallback(const BoolAndErrorCallback& callback,
                                   bool found,
                                   CacheStorageError error);
  void PendingCacheAndErrorCallback(
      const CacheAndErrorCallback& callback,
      const scoped_refptr<ServiceWorkerCache>& cache,
      CacheStorageError error);
  void PendingStringsAndErrorCallback(const StringsAndErrorCallback& callback,
                                      const StringVector& strings,
                                      CacheStorageError error);
  void PendingResponseCallback(
      const ServiceWorkerCache::ResponseCallback& callback,
      ServiceWorkerCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_;
  bool initializing_;

  // The pending operation scheduler.
  scoped_ptr<ServiceWorkerCacheScheduler> scheduler_;

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
