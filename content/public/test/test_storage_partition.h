// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_
#define CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "build/build_config.h"
#include "content/public/browser/storage_partition.h"

namespace net {
class URLRequestContextGetter;
}

namespace content {

class AppCacheService;
class DOMStorageContext;
class IndexedDBContext;
class PlatformNotificationContext;
class ServiceWorkerContext;

#if !defined(OS_ANDROID)
class HostZoomLevelContext;
class HostZoomMap;
class ZoomLevelDelegate;
#endif  // !defined(OS_ANDROID)

namespace mojom {
class NetworkContext;
}

// Fake implementation of StoragePartition.
class TestStoragePartition : public StoragePartition {
 public:
  TestStoragePartition();
  ~TestStoragePartition() override;

  void set_path(base::FilePath file_path) { file_path_ = file_path; }
  base::FilePath GetPath() override;

  void set_url_request_context(net::URLRequestContextGetter* getter) {
    url_request_context_getter_ = getter;
  }
  net::URLRequestContextGetter* GetURLRequestContext() override;

  void set_media_url_request_context(net::URLRequestContextGetter* getter) {
    media_url_request_context_getter_ = getter;
  }
  net::URLRequestContextGetter* GetMediaURLRequestContext() override;

  void set_network_context(mojom::NetworkContext* context) {
    network_context_ = context;
  }
  mojom::NetworkContext* GetNetworkContext() override;

  void set_quota_manager(storage::QuotaManager* manager) {
    quota_manager_ = manager;
  }
  storage::QuotaManager* GetQuotaManager() override;

  void set_app_cache_service(AppCacheService* service) {
    app_cache_service_ = service;
  }
  AppCacheService* GetAppCacheService() override;

  void set_file_system_context(storage::FileSystemContext* context) {
    file_system_context_ = context;
  }
  storage::FileSystemContext* GetFileSystemContext() override;

  void set_database_tracker(storage::DatabaseTracker* tracker) {
    database_tracker_ = tracker;
  }
  storage::DatabaseTracker* GetDatabaseTracker() override;

  void set_dom_storage_context(DOMStorageContext* context) {
    dom_storage_context_ = context;
  }
  DOMStorageContext* GetDOMStorageContext() override;

  void set_indexed_db_context(IndexedDBContext* context) {
    indexed_db_context_ = context;
  }
  IndexedDBContext* GetIndexedDBContext() override;

  void set_service_worker_context(ServiceWorkerContext* context) {
    service_worker_context_ = context;
  }
  ServiceWorkerContext* GetServiceWorkerContext() override;

  void set_cache_storage_context(CacheStorageContext* context) {
    cache_storage_context_ = context;
  }
  CacheStorageContext* GetCacheStorageContext() override;

  void set_platform_notification_context(PlatformNotificationContext* context) {
    platform_notification_context_ = context;
  }
  PlatformNotificationContext* GetPlatformNotificationContext() override;

#if !defined(OS_ANDROID)
  void set_host_zoom_map(HostZoomMap* map) { host_zoom_map_ = map; }
  HostZoomMap* GetHostZoomMap() override;

  void set_host_zoom_level_context(HostZoomLevelContext* context) {
    host_zoom_level_context_ = context;
  }
  HostZoomLevelContext* GetHostZoomLevelContext() override;

  void set_zoom_level_delegate(ZoomLevelDelegate* delegate) {
    zoom_level_delegate_ = delegate;
  }
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
#endif  // !defined(OS_ANDROID)

  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin,
                          net::URLRequestContextGetter* rq_context,
                          const base::Closure& callback) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 const CookieMatcherFunction& cookie_matcher,
                 const base::Time begin,
                 const base::Time end,
                 const base::Closure& callback) override;

  void ClearHttpAndMediaCaches(
      const base::Time begin,
      const base::Time end,
      const base::Callback<bool(const GURL&)>& url_matcher,
      const base::Closure& callback) override;

  void Flush() override;

  void ClearBluetoothAllowedDevicesMapForTesting() override;

 private:
  base::FilePath file_path_;
  net::URLRequestContextGetter* url_request_context_getter_;
  net::URLRequestContextGetter* media_url_request_context_getter_;
  mojom::NetworkContext* network_context_;
  storage::QuotaManager* quota_manager_;
  AppCacheService* app_cache_service_;
  storage::FileSystemContext* file_system_context_;
  storage::DatabaseTracker* database_tracker_;
  DOMStorageContext* dom_storage_context_;
  IndexedDBContext* indexed_db_context_;
  ServiceWorkerContext* service_worker_context_;
  CacheStorageContext* cache_storage_context_;
  PlatformNotificationContext* platform_notification_context_;
#if !defined(OS_ANDROID)
  HostZoomMap* host_zoom_map_;
  HostZoomLevelContext* host_zoom_level_context_;
  ZoomLevelDelegate* zoom_level_delegate_;
#endif  // !defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(TestStoragePartition);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_
