// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_

#include "content/browser/cache_storage/cache_storage.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

class CacheStorageContextImpl;
class CacheStorageListener;

// Handles Cache Storage related messages sent to the browser process from
// child processes. One host instance exists per child process. All
// messages are processed on the IO thread. Currently, all messages are
// passed directly to the owned CacheStorageListener instance, which
// in turn dispatches calls to the CacheStorageManager owned
// by the CacheStorageContextImpl for the entire browsing context.
// TODO(jsbell): Merge this with CacheStorageListener crbug.com/439389
class CONTENT_EXPORT CacheStorageDispatcherHost : public BrowserMessageFilter {
 public:
  CacheStorageDispatcherHost();

  // Runs on UI thread.
  void Init(CacheStorageContextImpl* context);

  // BrowserMessageFilter implementation
  void OnDestruct() const override;
  bool OnMessageReceived(const IPC::Message& message) override;

 private:
  // Friends to allow OnDestruct() delegation
  friend class BrowserThread;
  friend class base::DeleteHelper<CacheStorageDispatcherHost>;

  ~CacheStorageDispatcherHost() override;

  // Called by Init() on IO thread.
  void CreateCacheListener(CacheStorageContextImpl* context);

  // The message receiver functions for the CacheStorage API:
  void OnCacheStorageHas(int thread_id,
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

  // CacheStorageManager callbacks
  void OnCacheStorageHasCallback(int thread_id,
                                 int request_id,
                                 bool has_cache,
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

  scoped_ptr<CacheStorageListener> cache_listener_;
  scoped_refptr<CacheStorageContextImpl> context_;

  DISALLOW_COPY_AND_ASSIGN(CacheStorageDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_CACHE_STORAGE_CACHE_STORAGE_DISPATCHER_HOST_H_
