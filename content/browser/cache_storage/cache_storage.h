// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_

#include <stdint.h>

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/cache_storage/cache_storage_cache.h"

namespace base {
class SequencedTaskRunner;
}

namespace net {
class URLRequestContextGetter;
}

namespace storage {
class BlobStorageContext;
}

namespace content {
class CacheStorageScheduler;

// TODO(jkarlin): Constrain the total bytes used per origin.

// CacheStorage holds the set of caches for a given origin. It is
// owned by the CacheStorageManager. This class expects to be run
// on the IO thread. The asynchronous methods are executed serially.
class CONTENT_EXPORT CacheStorage {
 public:
  typedef std::vector<std::string> StringVector;
  typedef base::Callback<void(bool, CacheStorageError)> BoolAndErrorCallback;
  typedef base::Callback<void(const scoped_refptr<CacheStorageCache>&,
                              CacheStorageError)> CacheAndErrorCallback;
  typedef base::Callback<void(const StringVector&, CacheStorageError)>
      StringsAndErrorCallback;

  static const char kIndexFileName[];

  CacheStorage(
      const base::FilePath& origin_path,
      bool memory_only,
      base::SequencedTaskRunner* cache_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context,
      const GURL& origin);

  // Any unfinished asynchronous operations may not complete or call their
  // callbacks.
  virtual ~CacheStorage();

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
                  const CacheStorageCache::ResponseCallback& callback);

  // Calls match on all of the caches in parallel, calling |callback| with the
  // first response found. Note that if multiple caches have the same
  // request/response then it is not defined which cache's response will be
  // returned. If no response is found then |callback| is called with
  // CACHE_STORAGE_ERROR_NOT_FOUND.
  void MatchAllCaches(scoped_ptr<ServiceWorkerFetchRequest> request,
                      const CacheStorageCache::ResponseCallback& callback);

  // Calls close on each cache and runs the callback after all of them have
  // closed.
  void CloseAllCaches(const base::Closure& callback);

  // The size of all of the origin's contents in memory. Returns 0 if the cache
  // backend is not a memory backend. Runs synchronously.
  int64_t MemoryBackedSize() const;

  // The functions below are for tests to verify that the operations run
  // serially.
  void StartAsyncOperationForTesting();
  void CompleteAsyncOperationForTesting();

 private:
  friend class TestCacheStorage;

  class MemoryLoader;
  class SimpleCacheLoader;
  class CacheLoader;

  typedef std::map<std::string, base::WeakPtr<CacheStorageCache>> CacheMap;

  // Return a CacheStorageCache for the given name if the name is known. If the
  // CacheStorageCache has been deleted, creates a new one.
  scoped_refptr<CacheStorageCache> GetLoadedCache(
      const std::string& cache_name);

  // Holds a reference to a cache for thirty seconds.
  void TemporarilyPreserveCache(const scoped_refptr<CacheStorageCache>& cache);
  virtual void SchedulePreservedCacheRemoval(
      const base::Closure& callback);  // Virtual for testing.
  void RemovePreservedCache(const CacheStorageCache* cache);

  // Initializer and its callback are below.
  void LazyInit();
  void LazyInitImpl();
  void LazyInitDidLoadIndex(
      scoped_ptr<std::vector<std::string>> indexed_cache_names);

  // The Open and CreateCache callbacks are below.
  void OpenCacheImpl(const std::string& cache_name,
                     const CacheAndErrorCallback& callback);
  void CreateCacheDidCreateCache(const std::string& cache_name,
                                 const CacheAndErrorCallback& callback,
                                 const scoped_refptr<CacheStorageCache>& cache);
  void CreateCacheDidWriteIndex(const CacheAndErrorCallback& callback,
                                const scoped_refptr<CacheStorageCache>& cache,
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
                           const scoped_refptr<CacheStorageCache>& cache);
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
                      const CacheStorageCache::ResponseCallback& callback);
  void MatchCacheDidMatch(const scoped_refptr<CacheStorageCache>& cache,
                          const CacheStorageCache::ResponseCallback& callback,
                          CacheStorageError error,
                          scoped_ptr<ServiceWorkerResponse> response,
                          scoped_ptr<storage::BlobDataHandle> handle);

  // The MatchAllCaches callbacks are below.
  void MatchAllCachesImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                          const CacheStorageCache::ResponseCallback& callback);
  void MatchAllCachesDidMatch(scoped_refptr<CacheStorageCache> cache,
                              const base::Closure& barrier_closure,
                              CacheStorageCache::ResponseCallback* callback,
                              CacheStorageError error,
                              scoped_ptr<ServiceWorkerResponse> response,
                              scoped_ptr<storage::BlobDataHandle> handle);
  void MatchAllCachesDidMatchAll(
      scoped_ptr<CacheStorageCache::ResponseCallback> callback);

  // The CloseAllCaches callbacks are below.
  void CloseAllCachesImpl(const base::Closure& callback);

  void PendingClosure(const base::Closure& callback);
  void PendingBoolAndErrorCallback(const BoolAndErrorCallback& callback,
                                   bool found,
                                   CacheStorageError error);
  void PendingCacheAndErrorCallback(
      const CacheAndErrorCallback& callback,
      const scoped_refptr<CacheStorageCache>& cache,
      CacheStorageError error);
  void PendingStringsAndErrorCallback(const StringsAndErrorCallback& callback,
                                      const StringVector& strings,
                                      CacheStorageError error);
  void PendingResponseCallback(
      const CacheStorageCache::ResponseCallback& callback,
      CacheStorageError error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);

  // Whether or not we've loaded the list of cache names into memory.
  bool initialized_;
  bool initializing_;

  // The pending operation scheduler.
  scoped_ptr<CacheStorageScheduler> scheduler_;

  // The map of cache names to CacheStorageCache objects.
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

  // Holds ref pointers to recently opened caches so that they can be reused
  // without having the open the cache again.
  std::map<const CacheStorageCache*, scoped_refptr<CacheStorageCache>>
      preserved_caches_;

  base::WeakPtrFactory<CacheStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_H_
