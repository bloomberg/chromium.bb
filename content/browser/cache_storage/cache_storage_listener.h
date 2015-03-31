// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_LISTENER_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_LISTENER_H_

#include <list>
#include <map>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage_cache.h"
#include "content/browser/cache_storage/cache_storage_dispatcher_host.h"

namespace content {

struct CacheStorageBatchOperation;
struct CacheStorageCacheQueryParams;
struct ServiceWorkerFetchRequest;

// This class listens for requests on the Cache APIs, and sends response
// messages to the renderer process. There is one instance per
// CacheStorageDispatcherHost instance.
class CacheStorageListener {
 public:
  CacheStorageListener(CacheStorageDispatcherHost* dispatcher,
                       CacheStorageContextImpl* context);
  ~CacheStorageListener();

  bool OnMessageReceived(const IPC::Message& message);

 private:
  // The message receiver functions for the CacheStorage API:
  void OnCacheStorageGet(int thread_id,
                         int request_id,
                         const GURL& origin,
                         const base::string16& cache_name);
  void OnCacheStorageHas(int thread_id,
                         int request_id,
                         const GURL& origin,
                         const base::string16& cache_name);
  void OnCacheStorageCreate(int thread_id,
                            int request_id,
                            const GURL& origin,
                            const base::string16& cache_name);
  void OnCacheStorageOpen(int thread_id,
                          int request_id,
                          const GURL& origin,
                          const base::string16& cache_name);
  void OnCacheStorageDelete(int thread_id,
                            int request_id,
                            const GURL& origin,
                            const base::string16& cache_name);
  void OnCacheStorageKeys(int thread_id, int request_id, const GURL& origin);
  void OnCacheStorageMatch(int thread_id,
                           int request_id,
                           const GURL& origin,
                           const ServiceWorkerFetchRequest& request,
                           const CacheStorageCacheQueryParams& match_params);

  // The message receiver functions for the Cache API:
  void OnCacheMatch(int thread_id,
                    int request_id,
                    int cache_id,
                    const ServiceWorkerFetchRequest& request,
                    const CacheStorageCacheQueryParams& match_params);
  void OnCacheMatchAll(int thread_id,
                       int request_id,
                       int cache_id,
                       const ServiceWorkerFetchRequest& request,
                       const CacheStorageCacheQueryParams& match_params);
  void OnCacheKeys(int thread_id,
                   int request_id,
                   int cache_id,
                   const ServiceWorkerFetchRequest& request,
                   const CacheStorageCacheQueryParams& match_params);
  void OnCacheBatch(int thread_id,
                    int request_id,
                    int cache_id,
                    const std::vector<CacheStorageBatchOperation>& operations);
  void OnCacheClosed(int cache_id);
  void OnBlobDataHandled(const std::string& uuid);

 private:
  typedef int32_t CacheID;  // TODO(jkarlin): Bump to 64 bit.
  typedef std::map<CacheID, scoped_refptr<CacheStorageCache>> IDToCacheMap;
  typedef std::map<std::string, std::list<storage::BlobDataHandle>>
      UUIDToBlobDataHandleList;

  void Send(IPC::Message* message);

  // CacheStorageManager callbacks
  void OnCacheStorageGetCallback(int thread_id,
                                 int request_id,
                                 const scoped_refptr<CacheStorageCache>& cache,
                                 CacheStorage::CacheStorageError error);
  void OnCacheStorageHasCallback(int thread_id,
                                 int request_id,
                                 bool has_cache,
                                 CacheStorage::CacheStorageError error);
  void OnCacheStorageCreateCallback(
      int thread_id,
      int request_id,
      const scoped_refptr<CacheStorageCache>& cache,
      CacheStorage::CacheStorageError error);
  void OnCacheStorageOpenCallback(int thread_id,
                                  int request_id,
                                  const scoped_refptr<CacheStorageCache>& cache,
                                  CacheStorage::CacheStorageError error);
  void OnCacheStorageDeleteCallback(int thread_id,
                                    int request_id,
                                    bool deleted,
                                    CacheStorage::CacheStorageError error);
  void OnCacheStorageKeysCallback(int thread_id,
                                  int request_id,
                                  const std::vector<std::string>& strings,
                                  CacheStorage::CacheStorageError error);
  void OnCacheStorageMatchCallback(
      int thread_id,
      int request_id,
      CacheStorageCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);

  // Cache callbacks
  void OnCacheMatchCallback(
      int thread_id,
      int request_id,
      const scoped_refptr<CacheStorageCache>& cache,
      CacheStorageCache::ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);
  void OnCacheKeysCallback(int thread_id,
                           int request_id,
                           const scoped_refptr<CacheStorageCache>& cache,
                           CacheStorageCache::ErrorType error,
                           scoped_ptr<CacheStorageCache::Requests> requests);
  void OnCacheDeleteCallback(int thread_id,
                             int request_id,
                             const scoped_refptr<CacheStorageCache>& cache,
                             CacheStorageCache::ErrorType error);
  void OnCachePutCallback(int thread_id,
                          int request_id,
                          const scoped_refptr<CacheStorageCache>& cache,
                          CacheStorageCache::ErrorType error,
                          scoped_ptr<ServiceWorkerResponse> response,
                          scoped_ptr<storage::BlobDataHandle> blob_data_handle);

  // Hangs onto a scoped_refptr for the cache if it isn't already doing so.
  // Returns a unique cache_id. Call DropCacheReference when the client is done
  // with this cache.
  CacheID StoreCacheReference(const scoped_refptr<CacheStorageCache>& cache);
  void DropCacheReference(CacheID cache_id);

  // Stores blob handles while waiting for acknowledgement of receipt from the
  // renderer.
  void StoreBlobDataHandle(
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);
  void DropBlobDataHandle(std::string uuid);

  // Pointer to the context that owns this instance.
  CacheStorageDispatcherHost* dispatcher_;

  scoped_refptr<CacheStorageContextImpl> context_;

  IDToCacheMap id_to_cache_map_;
  CacheID next_cache_id_ = 0;

  UUIDToBlobDataHandleList blob_handle_store_;

  base::WeakPtrFactory<CacheStorageListener> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageListener);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_LISTENER_H_
