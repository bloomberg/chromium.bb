// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/test_storage_partition.h"

namespace content {

TestStoragePartition::TestStoragePartition() {}
TestStoragePartition::~TestStoragePartition() {}

base::FilePath TestStoragePartition::GetPath() {
  return file_path_;
}

net::URLRequestContextGetter* TestStoragePartition::GetURLRequestContext() {
  return url_request_context_getter_;
}

net::URLRequestContextGetter*
TestStoragePartition::GetMediaURLRequestContext() {
  return media_url_request_context_getter_;
}

mojom::NetworkContext* TestStoragePartition::GetNetworkContext() {
  return network_context_;
}

storage::QuotaManager* TestStoragePartition::GetQuotaManager() {
  return quota_manager_;
}

AppCacheService* TestStoragePartition::GetAppCacheService() {
  return app_cache_service_;
}

storage::FileSystemContext* TestStoragePartition::GetFileSystemContext() {
  return file_system_context_;
}

storage::DatabaseTracker* TestStoragePartition::GetDatabaseTracker() {
  return database_tracker_;
}

DOMStorageContext* TestStoragePartition::GetDOMStorageContext() {
  return dom_storage_context_;
}

IndexedDBContext* TestStoragePartition::GetIndexedDBContext() {
  return indexed_db_context_;
}

ServiceWorkerContext* TestStoragePartition::GetServiceWorkerContext() {
  return service_worker_context_;
}

CacheStorageContext* TestStoragePartition::GetCacheStorageContext() {
  return cache_storage_context_;
}

PlatformNotificationContext*
TestStoragePartition::GetPlatformNotificationContext() {
  return nullptr;
}

#if !defined(OS_ANDROID)
HostZoomMap* TestStoragePartition::GetHostZoomMap() {
  return host_zoom_map_;
}

HostZoomLevelContext* TestStoragePartition::GetHostZoomLevelContext() {
  return host_zoom_level_context_;
}

ZoomLevelDelegate* TestStoragePartition::GetZoomLevelDelegate() {
  return zoom_level_delegate_;
}
#endif  // !defined(OS_ANDROID)

void TestStoragePartition::ClearDataForOrigin(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    net::URLRequestContextGetter* rq_context,
    const base::Closure& callback) {}

void TestStoragePartition::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const GURL& storage_origin,
    const OriginMatcherFunction& origin_matcher,
    const base::Time begin,
    const base::Time end,
    const base::Closure& callback) {}

void TestStoragePartition::ClearData(
    uint32_t remove_mask,
    uint32_t quota_storage_remove_mask,
    const OriginMatcherFunction& origin_matcher,
    const CookieMatcherFunction& cookie_matcher,
    const base::Time begin,
    const base::Time end,
    const base::Closure& callback) {}

void TestStoragePartition::ClearHttpAndMediaCaches(
    const base::Time begin,
    const base::Time end,
    const base::Callback<bool(const GURL&)>& url_matcher,
    const base::Closure& callback) {}

void TestStoragePartition::Flush() {}

void TestStoragePartition::ClearBluetoothAllowedDevicesMapForTesting() {}

}  // namespace content
