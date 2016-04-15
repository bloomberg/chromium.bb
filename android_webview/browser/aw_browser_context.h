// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <memory>
#include <vector>

#include "android_webview/browser/aw_download_manager_delegate.h"
#include "android_webview/browser/aw_message_port_service.h"
#include "android_webview/browser/aw_ssl_host_state_delegate.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;
class PrefService;

namespace content {
class PermissionManager;
class ResourceContext;
class SSLHostStateDelegate;
class WebContents;
}

namespace data_reduction_proxy {
class DataReductionProxyConfigurator;
class DataReductionProxyIOData;
class DataReductionProxyService;
class DataReductionProxySettings;
}

namespace policy {
class URLBlacklistManager;
class BrowserPolicyConnectorBase;
}

namespace visitedlink {
class VisitedLinkMaster;
}

namespace android_webview {

class AwFormDatabaseService;
class AwQuotaManagerBridge;
class AwURLRequestContextGetter;
class JniDependencyFactory;

namespace prefs {

// Used for Kerberos authentication.
extern const char kAuthAndroidNegotiateAccountType[];
extern const char kAuthServerWhitelist[];

}  // namespace prefs

class AwBrowserContext : public content::BrowserContext,
                         public visitedlink::VisitedLinkDelegate {
 public:

  AwBrowserContext(const base::FilePath path,
                   JniDependencyFactory* native_factory);
  ~AwBrowserContext() override;

  // Currently only one instance per process is supported.
  static AwBrowserContext* GetDefault();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  static void SetDataReductionProxyEnabled(bool enabled);
  static void SetLegacyCacheRemovalDelayForTest(int delay_ms);

  // Maps to BrowserMainParts::PreMainMessageLoopRun.
  void PreMainMessageLoopRun();

  // These methods map to Add methods in visitedlink::VisitedLinkMaster.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  AwQuotaManagerBridge* GetQuotaManagerBridge();
  AwFormDatabaseService* GetFormDatabaseService();
  data_reduction_proxy::DataReductionProxySettings*
      GetDataReductionProxySettings();
  data_reduction_proxy::DataReductionProxyIOData*
      GetDataReductionProxyIOData();
  AwURLRequestContextGetter* GetAwURLRequestContext();
  AwMessagePortService* GetMessagePortService();

  policy::URLBlacklistManager* GetURLBlacklistManager();

  // content::BrowserContext implementation.
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath& partition_path) override;
  base::FilePath GetPath() const override;
  bool IsOffTheRecord() const override;
  content::ResourceContext* GetResourceContext() override;
  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override;
  content::BrowserPluginGuestManager* GetGuestManager() override;
  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override;
  content::PushMessagingService* GetPushMessagingService() override;
  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override;
  content::PermissionManager* GetPermissionManager() override;
  content::BackgroundSyncController* GetBackgroundSyncController() override;
  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  net::URLRequestContextGetter* CreateMediaRequestContext() override;
  net::URLRequestContextGetter* CreateMediaRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory) override;

  // visitedlink::VisitedLinkDelegate implementation.
  void RebuildTable(const scoped_refptr<URLEnumerator>& enumerator) override;

 private:
  void InitUserPrefService();
  void CreateDataReductionProxyStatisticsIfNecessary();
  static bool data_reduction_proxy_enabled_;

  // Delay, in milliseconds, before removing the legacy cache dir.
  // This is non-const for testing purposes.
  static int legacy_cache_removal_delay_ms_;

  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;

  JniDependencyFactory* native_factory_;
  scoped_refptr<AwURLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<AwQuotaManagerBridge> quota_manager_bridge_;
  std::unique_ptr<AwFormDatabaseService> form_database_service_;
  std::unique_ptr<AwMessagePortService> message_port_service_;

  AwDownloadManagerDelegate download_manager_delegate_;

  std::unique_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  std::unique_ptr<content::ResourceContext> resource_context_;

  std::unique_ptr<PrefService> user_pref_service_;
  std::unique_ptr<policy::BrowserPolicyConnectorBase> browser_policy_connector_;
  std::unique_ptr<policy::URLBlacklistManager> blacklist_manager_;

  std::unique_ptr<data_reduction_proxy::DataReductionProxySettings>
      data_reduction_proxy_settings_;
  std::unique_ptr<AwSSLHostStateDelegate> ssl_host_state_delegate_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyIOData>
      data_reduction_proxy_io_data_;
  std::unique_ptr<data_reduction_proxy::DataReductionProxyService>
      data_reduction_proxy_service_;
  std::unique_ptr<content::PermissionManager> permission_manager_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserContext);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
