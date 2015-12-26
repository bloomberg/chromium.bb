// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_manager.h"

#include <stdint.h>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/id_map.h"
#include "base/sha1.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "content/browser/cache_storage/cache_storage.h"
#include "content/browser/cache_storage/cache_storage.pb.h"
#include "content/browser/cache_storage/cache_storage_quota_client.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/net_util.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/common/database/database_identifier.h"
#include "storage/common/quota/quota_status_code.h"
#include "url/gurl.h"

namespace content {

namespace {

bool DeleteDir(const base::FilePath& path) {
  return base::DeleteFile(path, true /* recursive */);
}

void DeleteOriginDidDeleteDir(
    const storage::QuotaClient::DeletionCallback& callback,
    bool rv) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  callback.Run(rv ? storage::kQuotaStatusOk : storage::kQuotaErrorAbort);
}

std::set<GURL> ListOriginsOnDisk(base::FilePath root_path) {
  std::set<GURL> origins;
  base::FileEnumerator file_enum(root_path, false /* recursive */,
                                 base::FileEnumerator::DIRECTORIES);

  base::FilePath path;
  while (!(path = file_enum.Next()).empty()) {
    std::string protobuf;
    base::ReadFileToString(path.AppendASCII(CacheStorage::kIndexFileName),
                           &protobuf);

    CacheStorageIndex index;
    if (index.ParseFromString(protobuf)) {
      if (index.has_origin())
        origins.insert(GURL(index.origin()));
    }
  }

  return origins;
}

std::vector<CacheStorageUsageInfo> GetAllOriginsUsageOnTaskRunner(
    const base::FilePath root_path) {
  std::vector<CacheStorageUsageInfo> entries;
  const std::set<GURL> origins = ListOriginsOnDisk(root_path);
  entries.reserve(origins.size());
  for (const GURL& origin : origins) {
    base::FilePath path =
        CacheStorageManager::ConstructOriginPath(root_path, origin);
    int64_t size = base::ComputeDirectorySize(path);
    base::File::Info file_info;
    base::Time last_modified;
    if (base::GetFileInfo(path, &file_info))
      last_modified = file_info.last_modified;
    entries.push_back(CacheStorageUsageInfo(origin, size, last_modified));
  }
  return entries;
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

void EmptyQuotaStatusCallback(storage::QuotaStatusCode code) {}

}  // namespace

// static
scoped_ptr<CacheStorageManager> CacheStorageManager::Create(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy) {
  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
                    .AppendASCII("CacheStorage");
  }

  return make_scoped_ptr(new CacheStorageManager(root_path, cache_task_runner,
                                                 quota_manager_proxy));
}

// static
scoped_ptr<CacheStorageManager> CacheStorageManager::Create(
    CacheStorageManager* old_manager) {
  scoped_ptr<CacheStorageManager> manager(new CacheStorageManager(
      old_manager->root_path(), old_manager->cache_task_runner(),
      old_manager->quota_manager_proxy_.get()));
  // These values may be NULL, in which case this will be called again later by
  // the dispatcher host per usual.
  manager->SetBlobParametersForCache(old_manager->url_request_context_getter(),
                                     old_manager->blob_storage_context());
  return manager;
}

CacheStorageManager::~CacheStorageManager() = default;

void CacheStorageManager::OpenCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::CacheAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->OpenCache(cache_name, callback);
}

void CacheStorageManager::HasCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);
  cache_storage->HasCache(cache_name, callback);
}

void CacheStorageManager::DeleteCache(
    const GURL& origin,
    const std::string& cache_name,
    const CacheStorage::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);
  cache_storage->DeleteCache(cache_name, callback);
}

void CacheStorageManager::EnumerateCaches(
    const GURL& origin,
    const CacheStorage::StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->EnumerateCaches(callback);
}

void CacheStorageManager::MatchCache(
    const GURL& origin,
    const std::string& cache_name,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->MatchCache(cache_name, std::move(request), callback);
}

void CacheStorageManager::MatchAllCaches(
    const GURL& origin,
    scoped_ptr<ServiceWorkerFetchRequest> request,
    const CacheStorageCache::ResponseCallback& callback) {
  CacheStorage* cache_storage = FindOrCreateCacheStorage(origin);

  cache_storage->MatchAllCaches(std::move(request), callback);
}

void CacheStorageManager::SetBlobParametersForCache(
    const scoped_refptr<net::URLRequestContextGetter>& request_context_getter,
    base::WeakPtr<storage::BlobStorageContext> blob_storage_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(cache_storage_map_.empty());
  DCHECK(!request_context_getter_ ||
         request_context_getter_.get() == request_context_getter.get());
  DCHECK(!blob_context_ || blob_context_.get() == blob_storage_context.get());
  request_context_getter_ = request_context_getter;
  blob_context_ = blob_storage_context;
}

void CacheStorageManager::GetAllOriginsUsage(
    const CacheStorageContext::GetUsageInfoCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::vector<CacheStorageUsageInfo> entries;
    entries.reserve(cache_storage_map_.size());
    for (const auto& origin_details : cache_storage_map_) {
      entries.push_back(CacheStorageUsageInfo(
          origin_details.first, origin_details.second->MemoryBackedSize(),
          base::Time()));
    }
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(callback, entries));
    return;
  }

  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::Bind(&GetAllOriginsUsageOnTaskRunner, root_path_),
      base::Bind(callback));
}

void CacheStorageManager::GetOriginUsage(
    const GURL& origin_url,
    const storage::QuotaClient::GetUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    int64_t usage = 0;
    if (ContainsKey(cache_storage_map_, origin_url))
      usage = cache_storage_map_[origin_url]->MemoryBackedSize();
    callback.Run(usage);
    return;
  }

  MigrateOrigin(origin_url);
  PostTaskAndReplyWithResult(
      cache_task_runner_.get(), FROM_HERE,
      base::Bind(base::ComputeDirectorySize,
                 ConstructOriginPath(root_path_, origin_url)),
      base::Bind(callback));
}

void CacheStorageManager::GetOrigins(
    const storage::QuotaClient::GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (IsMemoryBacked()) {
    std::set<GURL> origins;
    for (const auto& key_value : cache_storage_map_)
      origins.insert(key_value.first);

    callback.Run(origins);
    return;
  }

  PostTaskAndReplyWithResult(cache_task_runner_.get(), FROM_HERE,
                             base::Bind(&ListOriginsOnDisk, root_path_),
                             base::Bind(callback));
}

void CacheStorageManager::GetOriginsForHost(
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
      cache_task_runner_.get(), FROM_HERE,
      base::Bind(&ListOriginsOnDisk, root_path_),
      base::Bind(&GetOriginsForHostDidListOrigins, host, callback));
}

void CacheStorageManager::DeleteOriginData(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  CacheStorageMap::iterator it = cache_storage_map_.find(origin);
  if (it == cache_storage_map_.end()) {
    callback.Run(storage::kQuotaStatusOk);
    return;
  }

  CacheStorage* cache_storage = it->second.release();
  cache_storage_map_.erase(origin);
  cache_storage->CloseAllCaches(
      base::Bind(&CacheStorageManager::DeleteOriginDidClose, origin, callback,
                 base::Passed(make_scoped_ptr(cache_storage)),
                 weak_ptr_factory_.GetWeakPtr()));
}

void CacheStorageManager::DeleteOriginData(const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DeleteOriginData(origin, base::Bind(&EmptyQuotaStatusCallback));
}

// static
void CacheStorageManager::DeleteOriginDidClose(
    const GURL& origin,
    const storage::QuotaClient::DeletionCallback& callback,
    scoped_ptr<CacheStorage> cache_storage,
    base::WeakPtr<CacheStorageManager> cache_manager) {
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

  cache_manager->MigrateOrigin(origin);
  PostTaskAndReplyWithResult(
      cache_manager->cache_task_runner_.get(), FROM_HERE,
      base::Bind(&DeleteDir,
                 ConstructOriginPath(cache_manager->root_path_, origin)),
      base::Bind(&DeleteOriginDidDeleteDir, callback));
}

CacheStorageManager::CacheStorageManager(
    const base::FilePath& path,
    const scoped_refptr<base::SequencedTaskRunner>& cache_task_runner,
    const scoped_refptr<storage::QuotaManagerProxy>& quota_manager_proxy)
    : root_path_(path),
      cache_task_runner_(cache_task_runner),
      quota_manager_proxy_(quota_manager_proxy),
      weak_ptr_factory_(this) {
  if (quota_manager_proxy_.get()) {
    quota_manager_proxy_->RegisterClient(
        new CacheStorageQuotaClient(weak_ptr_factory_.GetWeakPtr()));
  }
}

CacheStorage* CacheStorageManager::FindOrCreateCacheStorage(
    const GURL& origin) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(request_context_getter_);
  CacheStorageMap::const_iterator it = cache_storage_map_.find(origin);
  if (it == cache_storage_map_.end()) {
    MigrateOrigin(origin);
    CacheStorage* cache_storage = new CacheStorage(
        ConstructOriginPath(root_path_, origin), IsMemoryBacked(),
        cache_task_runner_.get(), request_context_getter_, quota_manager_proxy_,
        blob_context_, origin);
    cache_storage_map_.insert(
        std::make_pair(origin, make_scoped_ptr(cache_storage)));
    return cache_storage;
  }
  return it->second.get();
}

// static
base::FilePath CacheStorageManager::ConstructLegacyOriginPath(
    const base::FilePath& root_path,
    const GURL& origin) {
  const std::string origin_hash = base::SHA1HashString(origin.spec());
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

// static
base::FilePath CacheStorageManager::ConstructOriginPath(
    const base::FilePath& root_path,
    const GURL& origin) {
  const std::string identifier = storage::GetIdentifierFromOrigin(origin);
  const std::string origin_hash = base::SHA1HashString(identifier);
  const std::string origin_hash_hex = base::ToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

// Migrate from old origin-based path to storage identifier-based path.
// TODO(jsbell); Remove after a few releases.
void CacheStorageManager::MigrateOrigin(const GURL& origin) {
  if (IsMemoryBacked())
    return;
  base::FilePath old_path = ConstructLegacyOriginPath(root_path_, origin);
  base::FilePath new_path = ConstructOriginPath(root_path_, origin);
  cache_task_runner_->PostTask(
      FROM_HERE, base::Bind(&MigrateOriginOnTaskRunner, old_path, new_path));
}

// static
void CacheStorageManager::MigrateOriginOnTaskRunner(
    const base::FilePath& old_path,
    const base::FilePath& new_path) {
  if (base::PathExists(old_path)) {
    if (!base::PathExists(new_path))
      base::Move(old_path, new_path);
    base::DeleteFile(old_path, /*recursive*/ true);
  }
}

}  // namespace content
