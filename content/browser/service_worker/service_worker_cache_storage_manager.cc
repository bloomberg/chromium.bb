// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache_storage_manager.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/id_map.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_cache.pb.h"
#include "content/browser/service_worker/service_worker_cache_quota_client.h"
#include "content/browser/service_worker/service_worker_cache_storage.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/quota/quota_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

base::FilePath ConstructOriginPath(const base::FilePath& root_path,
                                   const GURL& origin) {
  std::string origin_hash = base::SHA1HashString(origin.spec());
  std::string origin_hash_hex = base::StringToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

bool DeleteDir(const base::FilePath& path) {
  return base::DeleteFile(path, true /* recursive */);
}

void DeleteOriginDidDeleteDir(
    const storage::QuotaClient::DeletionCallback& callback,
    bool rv) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(rv ? storage::kQuotaStatusOk : storage::kQuotaErrorAbort);
}

std::set<GURL> ListOriginsOnDisk(base::FilePath root_path_) {
  std::set<GURL> origins;
  base::FileEnumerator file_enum(
      root_path_, false /* recursive */, base::FileEnumerator::DIRECTORIES);

  base::FilePath path;
  while (!(path = file_enum.Next()).empty()) {
    std::string protobuf;
    base::ReadFileToString(
        path.AppendASCII(ServiceWorkerCacheStorage::kIndexFileName), &protobuf);

    ServiceWorkerCacheStorageIndex index;
    if (index.ParseFromString(protobuf)) {
      if (index.has_origin())
        origins.insert(GURL(index.origin()));
    }
  }

  return origins;
}

void GetOriginsForHostDidListOrigins(
    const std::string& host,
    const storage::QuotaClient::GetOriginsCallback& callback,
    const std::set<GURL>& origins) {
  std::set<GURL> out_origins;
  for (const GURL& origin : origins) {
    if (host == net::GetHostOrSpecFromURL(origin))
      out_origins.insert(origin);
  }
  callback.Run(out_origins);
}

}  // namespace

// static
scoped_ptr<ServiceWorkerCacheStorageManager>
ServiceWorkerCacheStorageManager::Create(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy) {
  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
                    .AppendASCII("CacheStorage");
  }

  return make_scoped_ptr(new ServiceWorkerCacheStorageManager(
      root_path, cache_task_runner, quota_manager_proxy));
}

// static
scoped_ptr<ServiceWorkerCacheStorageManager>
ServiceWorkerCacheStorageManager::Create(
    ServiceWorkerCacheStorageManager* old_manager) {
  scoped_ptr<ServiceWorkerCacheStorageManager> manager(
      new ServiceWorkerCacheStorageManager(
          old_manager->root_path(),
          old_manager->cache_task_runner(),
          old_manager->quota_manager_proxy_.get()));
  // These values may be NULL, in which case this will be called again later by
  // the dispatcher host per usual.
  manager->SetBlobParametersForCache(old_manager->url_request_context(),
                                     old_manager->blob_storage_context());
  return manager.Pass();
}

ServiceWorkerCacheStorageManager::~ServiceWorkerCacheStorageManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (ServiceWorkerCacheStorageMap::iterator it = cache_storage_map_.begin();
       it != cache_storage_map_.end();
       ++it) {
    delete it->second;
  }
}

void ServiceWorkerCacheStorageManager::OpenCache(
    const GURL& origin,
    const std::string& cache_name,
    const ServiceWorkerCacheStorage::CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->OpenCache(cache_name, callback);
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

void ServiceWorkerCacheStorageManager::MatchCache(
    const GURL& origin,
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback) {
  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->MatchCache(cache_name, request.Pass(), callback);
}

void ServiceWorkerCacheStorageManager::MatchAllCaches(
    const GURL& origin,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const ServiceWorkerCache::ResponseCallback& callback) {
  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);

  cache_storage->MatchAllCaches(request.Pass(), callback);
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

void ServiceWorkerCacheStorageManager::GetOriginUsage(
    const GURL& origin_url,
    const storage::QuotaClient::GetUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    int64 sum = 0;
    for (const auto& key_value : cache_storage_map_)
      sum += key_value.second->MemoryBackedSize();
    callback.Run(sum);
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(),
      FROM_HERE,
      base::Bind(base::ComputeDirectorySize,
                 ConstructOriginPath(root_path_, origin_url)),
      base::Bind(callback));
}

void ServiceWorkerCacheStorageManager::GetOrigins(
    const storage::QuotaClient::GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::set<GURL> origins;
    for (const auto& key_value : cache_storage_map_)
      origins.insert(key_value.first);

    callback.Run(origins);
    return;
  }

  PostTaskAndReplyWithResult(cache_task_runner_.get(),
                             FROM_HERE,
                             base::Bind(&ListOriginsOnDisk, root_path_),
                             base::Bind(callback));
}

void ServiceWorkerCacheStorageManager::GetOriginsForHost(
    const std::string& host,
    const storage::QuotaClient::GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::set<GURL> origins;
    for (const auto& key_value : cache_storage_map_) {
      if (host == net::GetHostOrSpecFromURL(key_value.first))
        origins.insert(key_value.first);
    }
    callback.Run(origins);
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(),
      FROM_HERE,
      base::Bind(&ListOriginsOnDisk, root_path_),
      base::Bind(&GetOriginsForHostDidListOrigins, host, callback));
}

void ServiceWorkerCacheStorageManager::DeleteOriginData(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerCacheStorage* cache_storage =
      FindOrCreateServiceWorkerCacheManager(origin);
  cache_storage_map_.erase(origin);
  cache_storage->CloseAllCaches(
      base::Bind(&ServiceWorkerCacheStorageManager::DeleteOriginDidClose,
                 origin, callback, base::Passed(make_scoped_ptr(cache_storage)),
                 weak_ptr_factory_.GetWeakPtr()));
}

// static
void ServiceWorkerCacheStorageManager::DeleteOriginDidClose(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback,
    scoped_ptr<ServiceWorkerCacheStorage> cache_storage,
    base::WeakPtr<ServiceWorkerCacheStorageManager> cache_manager) {
  // TODO(jkarlin): Deleting the storage leaves any unfinished operations
  // hanging, resulting in unresolved promises. Fix this by guaranteeing that
  // callbacks are called in ServiceWorkerStorage.
  cache_storage.reset();

  if (!cache_manager) {
    callback.Run(storage::kQuotaErrorAbort);
    return;
  }

  if (cache_manager->IsMemoryBacked()) {
    callback.Run(storage::kQuotaStatusOk);
    return;
  }

  PostTaskAndReplyWithResult(
      cache_manager->cache_task_runner_.get(), FROM_HERE,
      base::Bind(&DeleteDir,
                 ConstructOriginPath(cache_manager->root_path_, origin)),
      base::Bind(&DeleteOriginDidDeleteDir, callback));
}

ServiceWorkerCacheStorageManager::ServiceWorkerCacheStorageManager(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy)
    : root_path_(path),
      cache_task_runner_(cache_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      request_context_(NULL),
      weak_ptr_factory_(this) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->RegisterClient(
        new ServiceWorkerCacheQuotaClient(weak_ptr_factory_.GetWeakPtr()));
  }
}

ServiceWorkerCacheStorage*
ServiceWorkerCacheStorageManager::FindOrCreateServiceWorkerCacheManager(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_);
  ServiceWorkerCacheStorageMap::const_iterator it =
      cache_storage_map_.find(origin);
  if (it == cache_storage_map_.end()) {
    ServiceWorkerCacheStorage* cache_storage =
        new ServiceWorkerCacheStorage(ConstructOriginPath(root_path_, origin),
                                      IsMemoryBacked(),
                                      cache_task_runner_.get(),
                                      request_context_,
                                      quota_manager_proxy_,
                                      blob_context_,
                                      origin);
    // The map owns fetch_stores.
    cache_storage_map_.insert(std::make_pair(origin, cache_storage));
    return cache_storage;
  }
  return it->second;
}

}  // namespace content
