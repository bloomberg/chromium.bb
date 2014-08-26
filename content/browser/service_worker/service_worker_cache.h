// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_

#include "base/callback.h"
#include "base/files/file_path.h"
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
}

namespace content {
class ChromeBlobStorageContext;

// TODO(jkarlin): Unload cache backend from memory once the cache object is no
// longer referenced in javascript.

// Represents a ServiceWorker Cache as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html.
// InitializeIfNeeded must be called before calling the other public members.
class CONTENT_EXPORT ServiceWorkerCache {
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
  static scoped_ptr<ServiceWorkerCache> CreateMemoryCache(
      const std::string& name,
      net::URLRequestContext* request_context,
      base::WeakPtr<storage::BlobStorageContext> blob_context);
  static scoped_ptr<ServiceWorkerCache> CreatePersistentCache(
      const base::FilePath& path,
      const std::string& name,
      net::URLRequestContext* request_context,
      base::WeakPtr<storage::BlobStorageContext> blob_context);

  virtual ~ServiceWorkerCache();

  // Loads the backend and calls the callback with the result (true for
  // success). This must be called before member functions that require a
  // backend are called. The callback will always be called.
  void CreateBackend(const ErrorCallback& callback);

  // Returns ErrorTypeNotFound if not found. The callback will always be called.
  // |request| must remain valid until the callback is called.
  void Match(ServiceWorkerFetchRequest* request,
             const ResponseCallback& callback);

  // Puts the request and response object in the cache. The response body (if
  // present) is stored in the cache, but not the request body. Returns
  // ErrorTypeOK on success. The callback will always be called. |request| and
  // |response| must remain valid until the callback is called.
  void Put(ServiceWorkerFetchRequest* request,
           ServiceWorkerResponse* response,
           const ErrorCallback& callback);

  // Returns ErrorNotFound if not found. Otherwise deletes and returns
  // ErrorTypeOK. The callback will always be called. |request| must remain
  // valid until the callback is called.
  void Delete(ServiceWorkerFetchRequest* request,
              const ErrorCallback& callback);

  // Call to determine if CreateBackend must be called.
  bool HasCreatedBackend() const;

  void set_backend(scoped_ptr<disk_cache::Backend> backend) {
    backend_ = backend.Pass();
  }
  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }
  int32 id() const { return id_; }
  void set_id(int32 id) { id_ = id; }

  base::WeakPtr<ServiceWorkerCache> AsWeakPtr();

 private:
  ServiceWorkerCache(const base::FilePath& path,
                     const std::string& name,
                     net::URLRequestContext* request_context,
                     base::WeakPtr<storage::BlobStorageContext> blob_context);

  scoped_ptr<disk_cache::Backend> backend_;
  base::FilePath path_;
  std::string name_;
  net::URLRequestContext* request_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context_;
  int32 id_;

  base::WeakPtrFactory<ServiceWorkerCache> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCache);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_H_
