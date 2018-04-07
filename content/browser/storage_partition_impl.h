// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
#define CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/background_sync/background_sync_context.h"
#include "content/browser/blob_storage/blob_url_loader_factory.h"
#include "content/browser/bluetooth/bluetooth_allowed_devices_map.h"
#include "content/browser/broadcast_channel/broadcast_channel_provider.h"
#include "content/browser/cache_storage/cache_storage_context_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/locks/lock_manager.h"
#include "content/browser/notifications/platform_notification_context_impl.h"
#include "content/browser/payments/payment_app_context_impl.h"
#include "content/browser/push_messaging/push_messaging_context.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/common/content_export.h"
#include "content/common/storage_partition_service.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "net/cookies/cookie_store.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "storage/browser/quota/special_storage_policy.h"

#if !defined(OS_ANDROID)
#include "content/browser/host_zoom_level_context.h"
#endif

namespace content {

class BackgroundFetchContext;
class BlobRegistryWrapper;
class BlobURLLoaderFactory;
class PrefetchURLLoaderService;
class WebPackageContextImpl;

class CONTENT_EXPORT StoragePartitionImpl
    : public StoragePartition,
      public mojom::StoragePartitionService {
 public:
  // It is guaranteed that storage partitions are destructed before the
  // browser context starts shutting down its corresponding IO thread residents
  // (e.g. resource context).
  ~StoragePartitionImpl() override;

  // Quota managed data uses a different bitmask for types than
  // StoragePartition uses. This method generates that mask.
  static int GenerateQuotaClientMask(uint32_t remove_mask);

  // This creates a CookiePredicate that matches all host (NOT domain) cookies
  // that match the host of |url|. This is intended to be used with
  // DeleteAllCreatedBetweenWithPredicateAsync.
  static net::CookieStore::CookiePredicate
  CreatePredicateForHostCookies(const GURL& url);

  // Allows overriding the URLLoaderFactory creation for
  // GetURLLoaderFactoryForBrowserProcess.
  // Passing a null callback will restore the default behavior.
  // This method must be called either on the UI thread or before threads start.
  // This callback is run on the UI thread.
  using CreateNetworkFactoryCallback =
      base::Callback<network::mojom::URLLoaderFactoryPtr(
          network::mojom::URLLoaderFactoryPtr original_factory)>;
  static void SetGetURLLoaderFactoryForBrowserProcessCallbackForTesting(
      const CreateNetworkFactoryCallback& url_loader_factory_callback);

  void OverrideQuotaManagerForTesting(
      storage::QuotaManager* quota_manager);
  void OverrideSpecialStoragePolicyForTesting(
      storage::SpecialStoragePolicy* special_storage_policy);

  // StoragePartition interface.
  base::FilePath GetPath() override;
  net::URLRequestContextGetter* GetURLRequestContext() override;
  net::URLRequestContextGetter* GetMediaURLRequestContext() override;
  network::mojom::NetworkContext* GetNetworkContext() override;
  scoped_refptr<network::SharedURLLoaderFactory>
  GetURLLoaderFactoryForBrowserProcess() override;
  std::unique_ptr<network::SharedURLLoaderFactoryInfo>
  GetURLLoaderFactoryForBrowserProcessIOThread() override;
  network::mojom::CookieManager* GetCookieManagerForBrowserProcess() override;
  storage::QuotaManager* GetQuotaManager() override;
  ChromeAppCacheService* GetAppCacheService() override;
  storage::FileSystemContext* GetFileSystemContext() override;
  storage::DatabaseTracker* GetDatabaseTracker() override;
  DOMStorageContextWrapper* GetDOMStorageContext() override;
  LockManager* GetLockManager();  // override; TODO: Add to interface
  IndexedDBContextImpl* GetIndexedDBContext() override;
  CacheStorageContextImpl* GetCacheStorageContext() override;
  ServiceWorkerContextWrapper* GetServiceWorkerContext() override;
  SharedWorkerServiceImpl* GetSharedWorkerService() override;
#if !defined(OS_ANDROID)
  HostZoomMap* GetHostZoomMap() override;
  HostZoomLevelContext* GetHostZoomLevelContext() override;
  ZoomLevelDelegate* GetZoomLevelDelegate() override;
#endif  // !defined(OS_ANDROID)
  PlatformNotificationContextImpl* GetPlatformNotificationContext() override;
  WebPackageContext* GetWebPackageContext() override;
  void ClearDataForOrigin(uint32_t remove_mask,
                          uint32_t quota_storage_remove_mask,
                          const GURL& storage_origin) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const GURL& storage_origin,
                 const OriginMatcherFunction& origin_matcher,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;
  void ClearData(uint32_t remove_mask,
                 uint32_t quota_storage_remove_mask,
                 const OriginMatcherFunction& origin_matcher,
                 const CookieMatcherFunction& cookie_matcher,
                 const base::Time begin,
                 const base::Time end,
                 base::OnceClosure callback) override;
  void ClearHttpAndMediaCaches(
      const base::Time begin,
      const base::Time end,
      const base::Callback<bool(const GURL&)>& url_matcher,
      base::OnceClosure callback) override;
  void Flush() override;
  void ClearBluetoothAllowedDevicesMapForTesting() override;
  void FlushNetworkInterfaceForTesting() override;
  void WaitForDeletionTasksForTesting() override;

  BackgroundFetchContext* GetBackgroundFetchContext();
  BackgroundSyncContext* GetBackgroundSyncContext();
  PaymentAppContextImpl* GetPaymentAppContext();
  BroadcastChannelProvider* GetBroadcastChannelProvider();
  BluetoothAllowedDevicesMap* GetBluetoothAllowedDevicesMap();
  BlobURLLoaderFactory* GetBlobURLLoaderFactory();
  BlobRegistryWrapper* GetBlobRegistry();
  PrefetchURLLoaderService* GetPrefetchURLLoaderService();

  // mojom::StoragePartitionService interface.
  void OpenLocalStorage(const url::Origin& origin,
                        mojom::LevelDBWrapperRequest request) override;
  void OpenSessionStorage(
      const std::string& namespace_id,
      mojom::SessionStorageNamespaceRequest request) override;

  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter() {
    return url_loader_factory_getter_;
  }

  // Can return nullptr while |this| is being destroyed.
  BrowserContext* browser_context() const;

  // Called by each renderer process once. Returns the id of the created
  // binding.
  mojo::BindingId Bind(
      int process_id,
      mojo::InterfaceRequest<mojom::StoragePartitionService> request);

  auto& bindings_for_testing() { return bindings_; }

  // When this StoragePartition is for guests (e.g., for a <webview> tag), this
  // is the site URL to use when creating a SiteInstance for a service worker.
  // Typically one would use the script URL of the service worker (e.g.,
  // "https://example.com/sw.js"), but if this StoragePartition is for guests,
  // one must use the "chrome-guest://blahblah" site URL to ensure that the
  // service worker stays in this StoragePartition. This is an empty GURL if
  // this StoragePartition is not for guests.
  void set_site_for_service_worker(const GURL& site_for_service_worker) {
    site_for_service_worker_ = site_for_service_worker;
  }
  const GURL& site_for_service_worker() const {
    return site_for_service_worker_;
  }

 private:
  class DataDeletionHelper;
  class QuotaManagedDataDeletionHelper;
  class NetworkContextOwner;
  class URLLoaderFactoryForBrowserProcess;

  friend class BackgroundSyncManagerTest;
  friend class BackgroundSyncServiceImplTest;
  friend class PaymentAppContentUnitTestBase;
  friend class StoragePartitionImplMap;
  friend class URLLoaderFactoryForBrowserProcess;
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionShaderClearTest, ClearShaderCache);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverBoth);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyTemporary);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverOnlyPersistent);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverNeither);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForeverSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedDataForLastWeek);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedUnprotectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedSpecificOrigin);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedProtectedOrigins);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveQuotaManagedIgnoreDevTools);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieLastHour);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest, RemoveCookieWithMatcher);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveUnprotectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveProtectedLocalStorageForever);
  FRIEND_TEST_ALL_PREFIXES(StoragePartitionImplTest,
                           RemoveLocalStorageForLastWeek);

  // |relative_partition_path| is the relative path under |profile_path| to the
  // StoragePartition's on-disk-storage.
  //
  // If |in_memory| is true, the |relative_partition_path| is (ab)used as a way
  // of distinguishing different in-memory partitions, but nothing is persisted
  // on to disk.
  static std::unique_ptr<StoragePartitionImpl> Create(
      BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path);

  StoragePartitionImpl(BrowserContext* browser_context,
                       const base::FilePath& partition_path,
                       storage::SpecialStoragePolicy* special_storage_policy);

  // We will never have both remove_origin be populated and a cookie_matcher.
  void ClearDataImpl(uint32_t remove_mask,
                     uint32_t quota_storage_remove_mask,
                     const GURL& remove_origin,
                     const OriginMatcherFunction& origin_matcher,
                     const CookieMatcherFunction& cookie_matcher,
                     const base::Time begin,
                     const base::Time end,
                     base::OnceClosure callback);

  void DeletionHelperDone(base::OnceClosure callback);

  // Used by StoragePartitionImplMap.
  //
  // TODO(ajwong): These should be taken in the constructor and in Create() but
  // because the URLRequestContextGetter still lives in Profile with a tangled
  // initialization, if we try to retrieve the URLRequestContextGetter()
  // before the default StoragePartition is created, we end up reentering the
  // construction and double-initializing.  For now, we retain the legacy
  // behavior while allowing StoragePartitionImpl to expose these accessors by
  // letting StoragePartitionImplMap call these two private settings at the
  // appropriate time.  These should move back into the constructor once
  // URLRequestContextGetter's lifetime is sorted out. We should also move the
  // PostCreateInitialization() out of StoragePartitionImplMap.
  void SetURLRequestContext(
      net::URLRequestContextGetter* url_request_context);
  void SetMediaURLRequestContext(
      net::URLRequestContextGetter* media_url_request_context);

  // Function used by the quota system to ask the embedder for the
  // storage configuration info.
  void GetQuotaSettings(storage::OptionalQuotaSettingsCallback callback);

  network::mojom::URLLoaderFactory*
  GetURLLoaderFactoryForBrowserProcessInternal();

  // |is_in_memory_| and |relative_partition_path_| are cached from
  // |StoragePartitionImpl::Create()| in order to re-create |NetworkContext|.
  bool is_in_memory_;
  base::FilePath relative_partition_path_;
  base::FilePath partition_path_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
  scoped_refptr<URLLoaderFactoryGetter> url_loader_factory_getter_;
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<storage::FileSystemContext> filesystem_context_;
  scoped_refptr<storage::DatabaseTracker> database_tracker_;
  scoped_refptr<DOMStorageContextWrapper> dom_storage_context_;
  scoped_refptr<LockManager> lock_manager_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
  scoped_refptr<CacheStorageContextImpl> cache_storage_context_;
  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  std::unique_ptr<SharedWorkerServiceImpl> shared_worker_service_;
  scoped_refptr<PushMessagingContext> push_messaging_context_;
  scoped_refptr<storage::SpecialStoragePolicy> special_storage_policy_;
#if !defined(OS_ANDROID)
  scoped_refptr<HostZoomLevelContext> host_zoom_level_context_;
#endif  // !defined(OS_ANDROID)
  scoped_refptr<PlatformNotificationContextImpl> platform_notification_context_;
  std::unique_ptr<WebPackageContextImpl> web_package_context_;
  scoped_refptr<BackgroundFetchContext> background_fetch_context_;
  scoped_refptr<BackgroundSyncContext> background_sync_context_;
  scoped_refptr<PaymentAppContextImpl> payment_app_context_;
  scoped_refptr<BroadcastChannelProvider> broadcast_channel_provider_;
  scoped_refptr<BluetoothAllowedDevicesMap> bluetooth_allowed_devices_map_;
  scoped_refptr<BlobURLLoaderFactory> blob_url_loader_factory_;
  scoped_refptr<BlobRegistryWrapper> blob_registry_;
  scoped_refptr<PrefetchURLLoaderService> prefetch_url_loader_service_;

  // BindingSet for StoragePartitionService, using the process id as the
  // binding context type. The process id can subsequently be used during
  // interface method calls to enforce security checks.
  mojo::BindingSet<mojom::StoragePartitionService, int> bindings_;

  // This is the NetworkContext used to
  // make requests for the StoragePartition. When the network service is
  // enabled, the underlying NetworkContext will be owned by the network
  // service. When it's disabled, the underlying NetworkContext may either be
  // provided by the embedder, or is created by the StoragePartition and owned
  // by |network_context_owner_|.
  network::mojom::NetworkContextPtr network_context_;

  scoped_refptr<URLLoaderFactoryForBrowserProcess>
      shared_url_loader_factory_for_browser_process_;

  // URLLoaderFactory/CookieManager for use in the browser process only.
  // See the method comment for
  // StoragePartition::GetURLLoaderFactoryForBrowserProcess() for
  // more details
  network::mojom::URLLoaderFactoryPtr url_loader_factory_for_browser_process_;
  ::network::mojom::CookieManagerPtr cookie_manager_for_browser_process_;

  // When the network service is disabled, a NetworkContext is created on the IO
  // thread that wraps access to the URLRequestContext.
  std::unique_ptr<NetworkContextOwner> network_context_owner_;

  // Raw pointer that should always be valid. The BrowserContext owns the
  // StoragePartitionImplMap which then owns StoragePartitionImpl. When the
  // BrowserContext is destroyed, |this| will be destroyed too.
  BrowserContext* browser_context_;

  // See comments for site_for_service_worker().
  GURL site_for_service_worker_;

  // Track number of running deletion. For test use only.
  int deletion_helpers_running_;

  // Called when all deletions are done. For test use only.
  base::OnceClosure on_deletion_helpers_done_callback_;

  base::WeakPtrFactory<StoragePartitionImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StoragePartitionImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_STORAGE_PARTITION_IMPL_H_
