// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_cache_storage.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

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

// Keeps track of a ServiceWorkerCacheStorage per origin. There is one
// ServiceWorkerCacheStorageManager per ServiceWorkerContextCore.
// TODO(jkarlin): Remove ServiceWorkerCacheStorage from memory once they're no
// longer in active use.
class CONTENT_EXPORT ServiceWorkerCacheStorageManager {
 public:
  static scoped_ptr<ServiceWorkerCacheStorageManager> Create(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner);

  static scoped_ptr<ServiceWorkerCacheStorageManager> Create(
      ServiceWorkerCacheStorageManager* old_manager);

  virtual ~ServiceWorkerCacheStorageManager();

  // Methods to support the CacheStorage spec. These methods call the
  // corresponding ServiceWorkerCacheStorage method on the appropriate thread.
  void CreateCache(
      const GURL& origin,
      const std::string& cache_name,
      const ServiceWorkerCacheStorage::CacheAndErrorCallback& callback);
  void GetCache(
      const GURL& origin,
      const std::string& cache_name,
      const ServiceWorkerCacheStorage::CacheAndErrorCallback& callback);
  void HasCache(
      const GURL& origin,
      const std::string& cache_name,
      const ServiceWorkerCacheStorage::BoolAndErrorCallback& callback);
  void DeleteCache(
      const GURL& origin,
      const std::string& cache_name,
      const ServiceWorkerCacheStorage::BoolAndErrorCallback& callback);
  void EnumerateCaches(
      const GURL& origin,
      const ServiceWorkerCacheStorage::StringsAndErrorCallback& callback);
  // TODO(jkarlin): Add match() function.

  // This must be called before creating any of the public *Cache functions
  // above.
  void SetBlobParametersForCache(
      net::URLRequestContext* request_context,
      base::WeakPtr<storage::BlobStorageContext> blob_storage_context);

 private:
  typedef std::map<GURL, ServiceWorkerCacheStorage*>
      ServiceWorkerCacheStorageMap;

  ServiceWorkerCacheStorageManager(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner);

  // The returned ServiceWorkerCacheStorage* is owned by
  // service_worker_cache_storages_.
  ServiceWorkerCacheStorage* FindOrCreateServiceWorkerCacheManager(
      const GURL& origin);

  net::URLRequestContext* url_request_context() const {
    return request_context_;
  }
  base::WeakPtr<storage::BlobStorageContext> blob_storage_context() const {
    return blob_context_;
  }
  base::FilePath root_path() const { return root_path_; }
  const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner() const {
    return cache_task_runner_;
  }

  base::FilePath root_path_;
  scoped_refptr<base::SequencedTaskRunner> cache_task_runner_;

  // The map owns the CacheStorages and the CacheStorages are only accessed on
  // |cache_task_runner_|.
  ServiceWorkerCacheStorageMap cache_storage_map_;

  net::URLRequestContext* request_context_;
  base::WeakPtr<storage::BlobStorageContext> blob_context_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerCacheStorageManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_CACHE_STORAGE_MANAGER_H_
