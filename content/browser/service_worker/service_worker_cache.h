// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_

#include <list>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_worker/service_worker_types.h"
#include "net/base/completion_callback.h"
#include "net/disk_cache/disk_cache.h"

namespace net {
class URLRequestContext;
class IOBufferWithSize;
}

namespace storage {
class BlobDataBuilder;
class BlobDataHandle;
class BlobStorageContext;
class QuotaManagerProxy;
}

namespace content {
class ChromeBlobStorageContext;
class ServiceWorkerCacheMetadata;
class ServiceWorkerCacheScheduler;
class TestServiceWorkerCache;

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html.
// The asynchronous methods are executed serially. Callbacks to the
// public functions will be called so long as the cache object lives.
class CONTENT_EXPORT ServiceWorkerCache
    : public base::RefCounted<ServiceWorkerCache> {
 public:
  // This enum is used in histograms, so do not change the ordering and always
  // append new types to the end.
  enum ErrorType {
    ERROR_TYPE_OK = 0,
    ERROR_TYPE_EXISTS,
    ERROR_TYPE_STORAGE,
    ERROR_TYPE_NOT_FOUND,
    ERROR_TYPE_LAST = ERROR_TYPE_NOT_FOUND
  };

  enum EntryIndex { INDEX_HEADERS = 0, INDEX_RESPONSE_BODY };
  typedef base::Callback<void(ErrorType)> ErrorCallback;
  typedef base::Callback<void(ErrorType,
                              scoped_ptr<ServiceWorkerResponse>,
                              scoped_ptr<storage::BlobDataHandle>)>
      ResponseCallback;
  typedef std::vector<ServiceWorkerFetchRequest> Requests;
  typedef base::Callback<void(ErrorType, scoped_ptr<Requests>)>
      RequestsCallback;

  static scoped_refptr<ServiceWorkerCache> CreateMemoryCache(
      const GURL& origin,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);
  static scoped_refptr<ServiceWorkerCache> CreatePersistentCache(
      const GURL& origin,
      const base::FilePath& path,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Returns ERROR_TYPE_NOT_FOUND if not found.
  void Match(scoped_ptr<ServiceWorkerFetchRequest> request,
             const ResponseCallback& callback);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns
  // ERROR_TYPE_OK on success.
  void Put(scoped_ptr<ServiceWorkerFetchRequest> request,
           scoped_ptr<ServiceWorkerResponse> response,
           const ResponseCallback& callback);

  // Returns ErrorNotFound if not found. Otherwise deletes and returns
  // ERROR_TYPE_OK.
  void Delete(scoped_ptr<ServiceWorkerFetchRequest> request,
              const ErrorCallback& callback);

  // TODO(jkarlin): Have keys take an optional ServiceWorkerFetchRequest.
  // Returns ErrorTypeOK and a vector of requests if there are no errors.
  void Keys(const RequestsCallback& callback);

  // Closes the backend. Future operations that require the backend
  // will exit early. Close should only be called once per ServiceWorkerCache.
  void Close(const base::Closure& callback);

  // The size of the cache contents in memory. Returns 0 if the cache backend is
  // not a memory cache backend.
  int64 MemoryBackedSize() const;

  base::WeakPtr<ServiceWorkerCache> AsWeakPtr();

 private:
  friend class base::RefCounted<ServiceWorkerCache>;
  friend class TestServiceWorkerCache;

  class BlobReader;
  struct KeysContext;
  struct MatchContext;
  struct PutContext;

  // The backend progresses from uninitialized, to open, to closed, and cannot
  // reverse direction.  The open step may be skipped.
  enum BackendState {
    BACKEND_UNINITIALIZED,  // No backend, create backend on first operation.
    BACKEND_OPEN,           // Backend can be used.
    BACKEND_CLOSED          // Backend cannot be used.  All ops should fail.
  };

  typedef std::vector<disk_cache::Entry*> Entries;
  typedef scoped_ptr<disk_cache::Backend> ScopedBackendPtr;

  ServiceWorkerCache(
      const GURL& origin,
      const base::FilePath& path,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Async operations in progress will cancel and not run their callbacks.
  virtual ~ServiceWorkerCache();

  // Match callbacks
  void MatchImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                 const ResponseCallback& callback);
  void MatchDidOpenEntry(scoped_ptr<MatchContext> match_context, int rv);
  void MatchDidReadMetadata(scoped_ptr<MatchContext> match_context,
                            scoped_ptr<ServiceWorkerCacheMetadata> headers);
  void MatchDidReadResponseBodyData(scoped_ptr<MatchContext> match_context,
                                    int rv);
  void MatchDoneWithBody(scoped_ptr<MatchContext> match_context);

  // Put callbacks.
  void PutImpl(scoped_ptr<PutContext> put_context);
  void PutDidDelete(scoped_ptr<PutContext> put_context, ErrorType delete_error);
  void PutDidCreateEntry(scoped_ptr<PutContext> put_context, int rv);
  void PutDidWriteHeaders(scoped_ptr<PutContext> put_context,
                          int expected_bytes,
                          int rv);
  void PutDidWriteBlobToCache(scoped_ptr<PutContext> put_context,
                              scoped_ptr<BlobReader> blob_reader,
                              disk_cache::ScopedEntryPtr entry,
                              bool success);

  // Delete callbacks
  void DeleteImpl(scoped_ptr<ServiceWorkerFetchRequest> request,
                  const ErrorCallback& callback);
  void DeleteDidOpenEntry(
      const GURL& origin,
      scoped_ptr<ServiceWorkerFetchRequest> request,
      const ServiceWorkerCache::ErrorCallback& callback,
      scoped_ptr<disk_cache::Entry*> entryptr,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      int rv);

  // Keys callbacks.
  void KeysImpl(const RequestsCallback& callback);
  void KeysDidOpenNextEntry(scoped_ptr<KeysContext> keys_context, int rv);
  void KeysProcessNextEntry(scoped_ptr<KeysContext> keys_context,
                            const Entries::iterator& iter);
  void KeysDidReadMetadata(scoped_ptr<KeysContext> keys_context,
                           const Entries::iterator& iter,
                           scoped_ptr<ServiceWorkerCacheMetadata> metadata);

  void CloseImpl(const base::Closure& callback);

  // Loads the backend and calls the callback with the result (true for
  // success). The callback will always be called. Virtual for tests.
  virtual void CreateBackend(const ErrorCallback& callback);
  void CreateBackendDidCreate(const ServiceWorkerCache::ErrorCallback& callback,
                              scoped_ptr<ScopedBackendPtr> backend_ptr,
                              int rv);

  void InitBackend();
  void InitDone(ErrorType error);

  void PendingClosure(const base::Closure& callback);
  void PendingErrorCallback(const ErrorCallback& callback, ErrorType error);
  void PendingResponseCallback(
      const ResponseCallback& callback,
      ErrorType error,
      scoped_ptr<ServiceWorkerResponse> response,
      scoped_ptr<storage::BlobDataHandle> blob_data_handle);
  void PendingRequestsCallback(const RequestsCallback& callback,
                               ErrorType error,
                               scoped_ptr<Requests> requests);

  // Be sure to check |backend_state_| before use.
  scoped_ptr<disk_cache::Backend> backend_;

  GURL origin_;
  base::FilePath path_;
  net::URLRequestContext* request_context_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  BackendState backend_state_;
  scoped_ptr<ServiceWorkerCacheScheduler> scheduler_;
  bool initializing_;

  // Whether or not to store data in disk or memory.
  bool memory_only_;

  base::WeakPtrFactory<ServiceWorkerCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
