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

  void set_network_context(network::mojom::NetworkContext* context) {
    network_context_ = context;
  }
  network::mojom::NetworkContext* GetNetworkContext() override;

  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;

  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;

  void set_cookie_manager_for_browser_process(
      network::mojom::CookieManager* cookie_manager_for_browser_process) {
    cookie_manager_for_browser_process_ = cookie_manager_for_browser_process;
  }
  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override;

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

  void set_shared_worker_service(SharedWorkerService* service) {
    shared_worker_service_ = service;
  }
  SharedWorkerService* GetSharedWorkerService() override;

  void set_cache_storage_context(CacheStorageContext* context) {
    cache_storage_context_ = context;
  }
  CacheStorageContext* GetCacheStorageContext() override;

  void set_generated_code_cache_context(GeneratedCodeCacheContext* context) {
    generated_code_cache_context_ = context;
  }
  GeneratedCodeCacheContext* GetGeneratedCodeCacheContext() override;

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
                          const GURL& storage_origin) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;

  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 network::mojom::CookieDeletionFilterPtr cookie_deletion_filter,
                 bool perform_storage_cleanup,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;

  void ClearHttpAndMediaCaches(
      const base::Time begin,
      const base::Time end,
      const base::Callback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override;

  void ClearCodeCaches(base::OnceClosure callback) override;

  void Flush() override;

  void ResetURLLoaderFactories() override;

  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void FlushNetworkInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;

 private:
  base::FilePath file_path_;
  net::URLRequestContextGetter* url_request_context_getter_ = nullptr;
  net::URLRequestContextGetter* media_url_request_context_getter_ = nullptr;
  network::mojom::NetworkContext* network_context_ = nullptr;
  network::mojom::CookieManager* cookie_manager_for_browser_process_ = nullptr;
  storage::QuotaManager* quota_manager_ = nullptr;
  AppCacheService* app_cache_service_ = nullptr;
  storage::FileSystemContext* file_system_context_ = nullptr;
  storage::DatabaseTracker* database_tracker_ = nullptr;
  DOMStorageContext* dom_storage_context_ = nullptr;
  IndexedDBContext* indexed_db_context_ = nullptr;
  ServiceWorkerContext* service_worker_context_ = nullptr;
  SharedWorkerService* shared_worker_service_ = nullptr;
  CacheStorageContext* cache_storage_context_ = nullptr;
  GeneratedCodeCacheContext* generated_code_cache_context_ = nullptr;
  PlatformNotificationContext* platform_notification_context_ = nullptr;
#if !defined(OS_ANDROID)
  HostZoomMap* host_zoom_map_ = nullptr;
  HostZoomLevelContext* host_zoom_level_context_ = nullptr;
  ZoomLevelDelegate* zoom_level_delegate_ = nullptr;
#endif  // !defined(OS_ANDROID)

  DISALLOW_COPY_AND_ASSIGN(TestStoragePartition);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_TEST_STORAGE_PARTITION_H_
