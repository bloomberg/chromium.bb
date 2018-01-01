// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/cache_storage/cache_storage_quota_client.h"

#include "content/browser/cache_storage/cache_storage_manager.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/common/quota/quota_status_code.h"

namespace content {

CacheStorageQuotaClient::CacheStorageQuotaClient(
    base::WeakPtr<CacheStorageManager> cache_manager)
    : cache_manager_(cache_manager) {
}

CacheStorageQuotaClient::~CacheStorageQuotaClient() {
}

storage::QuotaClient::ID CacheStorageQuotaClient::id() const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  return kServiceWorkerCache;
}

void CacheStorageQuotaClient::OnQuotaManagerDestroyed() {
  delete this;
}

void CacheStorageQuotaClient::GetOriginUsage(const GURL& origin_url,
                                             blink::StorageType type,
                                             const GetUsageCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(0);
    return;
  }

  cache_manager_->GetOriginUsage(origin_url, callback);
}

void CacheStorageQuotaClient::GetOriginsForType(
    blink::StorageType type,
    const GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(std::set<GURL>());
    return;
  }

  cache_manager_->GetOrigins(callback);
}

void CacheStorageQuotaClient::GetOriginsForHost(
    blink::StorageType type,
    const std::string& host,
    const GetOriginsCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_ || !DoesSupport(type)) {
    callback.Run(std::set<GURL>());
    return;
  }

  cache_manager_->GetOriginsForHost(host, callback);
}

void CacheStorageQuotaClient::DeleteOriginData(
    const GURL& origin,
    blink::StorageType type,
    const DeletionCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!cache_manager_) {
    callback.Run(blink::QuotaStatusCode::kErrorAbort);
    return;
  }

  if (!DoesSupport(type)) {
    callback.Run(blink::QuotaStatusCode::kOk);
    return;
  }

  cache_manager_->DeleteOriginData(origin, callback);
}

bool CacheStorageQuotaClient::DoesSupport(blink::StorageType type) const {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  return type == blink::StorageType::kTemporary;
}

}  // namespace content
