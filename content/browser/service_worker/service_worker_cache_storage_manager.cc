// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage_manager.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/id_map.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache_storage.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

base::FilePath ConstructOriginPath(const base::FilePath& root_path,
                                   const GURL& origin) {
  std::string origin_hash = base::SHA1HashString(origin.spec());
  std::string origin_hash_hex = base::StringToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

}  // namespace

namespace content {

// static
scoped_ptr<ServiceWorkerCacheStorageManager>
ServiceWorkerCacheStorageManager::Create(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner) {
  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
                    .AppendASCII("CacheStorage");
  }

  return make_scoped_ptr(
      new ServiceWorkerCacheStorageManager(root_path, cache_task_runner));
}

// static
scoped_ptr<ServiceWorkerCacheStorageManager>
ServiceWorkerCacheStorageManager::Create(
    ServiceWorkerCacheStorageManager* old_manager) {
  scoped_ptr<ServiceWorkerCacheStorageManager> manager(
      new ServiceWorkerCacheStorageManager(old_manager->root_path(),
                                           old_manager->cache_task_runner()));
  // These values may be NULL, in which case this will be called again later by
  // the dispatcher host per usual.
  manager->SetBlobParametersForCache(old_manager->url_request_context(),
                                     old_manager->blob_storage_context());
  return manager.Pass();
}

ServiceWorkerCacheStorageManager::~ServiceWorkerCacheStorageManager() {
  for (ServiceWorkerCacheStorageMap::iterator it = cache_storage_map_.begin();
       it != cache_storage_map_.end();
       ++it) {
    delete it->second;
  }
}

void ServiceWorkerCacheStorageManager::CreateCache(
    const GURL& origin,
    const std::string& cache_name,
    const ServiceWorkerCacheStorage::CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->CreateCache(cache_name, callback);
}

void ServiceWorkerCacheStorageManager::GetCache(
    const GURL& origin,
    const std::string& cache_name,
    const ServiceWorkerCacheStorage::CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->GetCache(cache_name, callback);
}

void ServiceWorkerCacheStorageManager::HasCache(
    const GURL& origin,
    const std::string& cache_name,
    const ServiceWorkerCacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);
  cache_storage->HasCache(cache_name, callback);
}

void ServiceWorkerCacheStorageManager::DeleteCache(
    const GURL& origin,
    const std::string& cache_name,
    const ServiceWorkerCacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);
  cache_storage->DeleteCache(cache_name, callback);
}

void ServiceWorkerCacheStorageManager::EnumerateCaches(
    const GURL& origin,
    const ServiceWorkerCacheStorage::StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->EnumerateCaches(callback);
}

void ServiceWorkerCacheStorageManager::SetBlobParametersForCache(
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_storage_map_.empty());
  DCHECK(!request_context_ || request_context_ == request_context);
  DCHECK(!blob_context_ || blob_context_.get() == blob_storage_context.get());
  request_context_ = request_context;
  blob_context_ = blob_storage_context;
}

ServiceWorkerCacheStorageManager::ServiceWorkerCacheStorageManager(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner)
    : root_path_(path),
      cache_task_runner_(cache_task_runner),
      request_context_(NULL) {
}

ServiceWorkerCacheStorage*
ServiceWorkerCacheStorageManager::FindOrCreateServiceWorkerCacheManager(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_);

  ServiceWorkerCacheStorageMap::const_iterator it =
      cache_storage_map_.find(origin);
  if (it == cache_storage_map_.end()) {
    bool memory_only = root_path_.empty();
    ServiceWorkerCacheStorage* cache_storage =
        new ServiceWorkerCacheStorage(ConstructOriginPath(root_path_, origin),
                                      memory_only,
                                      cache_task_runner_.get(),
                                      request_context_,
                                      blob_context_);
    // The map owns fetch_stores.
    cache_storage_map_.insert(std::make_pair(origin, cache_storage));
    return cache_storage;
  }
  return it->second;
}

}  // namespace content
