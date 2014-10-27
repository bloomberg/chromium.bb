// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_

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
class BlobData;
class BlobDataHandle;
class BlobStorageContext;
class QuotaManagerProxy;
}

namespace content {
class ChromeBlobStorageContext;
class ServiceWorkerCacheMetadata;

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html.
// InitializeIfNeeded must be called before calling the other public members.
class CONTENT_EXPORT ServiceWorkerCache
    : public base::RefCounted<ServiceWorkerCache> {
 public:
  enum ErrorType {
    ErrorTypeOK = 0,
    ErrorTypeExists,
    ErrorTypeStorage,
    ErrorTypeNotFound
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

  // Returns ErrorTypeNotFound if not found. The callback will always be called.
  void Match(scoped_ptr<ServiceWorkerFetchRequest> request,
             const ResponseCallback& callback);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns
  // ErrorTypeOK on success. The callback will always be called.
  void Put(scoped_ptr<ServiceWorkerFetchRequest> request,
           scoped_ptr<ServiceWorkerResponse> response,
           const ResponseCallback& callback);

  // Returns ErrorNotFound if not found. Otherwise deletes and returns
  // ErrorTypeOK. The callback will always be called.
  void Delete(scoped_ptr<ServiceWorkerFetchRequest> request,
              const ErrorCallback& callback);

  // TODO(jkarlin): Have keys take an optional ServiceWorkerFetchRequest.
  // Returns ErrorTypeOK and a vector of requests if there are no errors. The
  // callback will always be called.
  void Keys(const RequestsCallback& callback);

  // Prevent further operations on this object and delete the backend.
  void Close();

  // The size of the cache contents in memory. Returns 0 if the cache backend is
  // not a memory cache backend.
  int64 MemoryBackedSize() const;

  void set_backend(scoped_ptr<disk_cache::Backend> backend) {
    backend_ = backend.Pass();
  }

  base::WeakPtr<ServiceWorkerCache> AsWeakPtr();

 private:
  friend class base::RefCounted<ServiceWorkerCache>;

  class BlobReader;
  struct KeysContext;
  struct PutContext;
  typedef std::vector<disk_cache::Entry*> Entries;

  ServiceWorkerCache(
      const GURL& origin,
      const base::FilePath& path,
      net::URLRequestContext* request_context,
      const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  // Operations in progress will complete after the cache is deleted but pending
  // operations (those operations waiting for init to finish) won't.
  virtual ~ServiceWorkerCache();

  // Put callbacks.
  static void PutImpl(scoped_ptr<PutContext> put_context);
  static void PutDidCreateEntry(scoped_ptr<PutContext> put_context, int rv);
  static void PutDidWriteHeaders(scoped_ptr<PutContext> put_context,
                                 int expected_bytes,
                                 int rv);
  static void PutDidWriteBlobToCache(scoped_ptr<PutContext> put_context,
                                     scoped_ptr<BlobReader> blob_reader,
                                     disk_cache::ScopedEntryPtr entry,
                                     bool success);

  // Static callbacks for the Keys function.
  static void KeysDidOpenNextEntry(scoped_ptr<KeysContext> keys_context,
                                   int rv);
  static void KeysProcessNextEntry(scoped_ptr<KeysContext> keys_context,
                                   const Entries::iterator& iter);
  static void KeysDidReadMetadata(
      scoped_ptr<KeysContext> keys_context,
      const Entries::iterator& iter,
      scoped_ptr<ServiceWorkerCacheMetadata> metadata);

  // Loads the backend and calls the callback with the result (true for
  // success). The callback will always be called.
  void CreateBackend(const ErrorCallback& callback);

  void Init(const base::Closure& callback);
  void InitDone(ErrorType error);

  // The backend can be deleted via the Close function at any time so always
  // check for its existence before use.
  scoped_ptr<disk_cache::Backend> backend_;
  GURL origin_;
  base::FilePath path_;
  net::URLRequestContext* request_context_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  bool initialized_;
  std::vector<base::Closure> init_callbacks_;

  // Whether or not to store data in disk or memory.
  bool memory_only_;

  base::WeakPtrFactory<ServiceWorkerCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
