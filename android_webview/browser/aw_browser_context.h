// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
#define ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_

#include <vector>

#include "android_webview/browser/aw_download_manager_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_job_factory.h"

class GURL;
class PrefService;

namespace content {
class ResourceContext;
class WebContents;
}

namespace data_reduction_proxy {
class DataReductionProxyAuthRequestHandler;
class DataReductionProxySettings;
}

namespace net {
class CookieStore;
}

namespace visitedlink {
class VisitedLinkMaster;
}

using data_reduction_proxy::DataReductionProxyAuthRequestHandler;
using data_reduction_proxy::DataReductionProxySettings;

namespace android_webview {

class AwFormDatabaseService;
class AwQuotaManagerBridge;
class AwURLRequestContextGetter;
class JniDependencyFactory;

class AwBrowserContext : public content::BrowserContext,
                         public visitedlink::VisitedLinkDelegate {
 public:

  AwBrowserContext(const base::FilePath path,
                   JniDependencyFactory* native_factory);
  virtual ~AwBrowserContext();

  // Currently only one instance per process is supported.
  static AwBrowserContext* GetDefault();

  // Convenience method to returns the AwBrowserContext corresponding to the
  // given WebContents.
  static AwBrowserContext* FromWebContents(
      content::WebContents* web_contents);

  static void SetDataReductionProxyEnabled(bool enabled);

  // Maps to BrowserMainParts::PreMainMessageLoopRun.
  void PreMainMessageLoopRun();

  // These methods map to Add methods in visitedlink::VisitedLinkMaster.
  void AddVisitedURLs(const std::vector<GURL>& urls);

  net::URLRequestContextGetter* CreateRequestContext(
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);
  net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors);

  AwQuotaManagerBridge* GetQuotaManagerBridge();

  AwFormDatabaseService* GetFormDatabaseService();

  DataReductionProxySettings* GetDataReductionProxySettings();

  DataReductionProxyAuthRequestHandler*
  GetDataReductionProxyAuthRequestHandler();

  void CreateUserPrefServiceIfNecessary();

  // content::BrowserContext implementation.
  virtual base::FilePath GetPath() const OVERRIDE;
  virtual bool IsOffTheRecord() const OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContext() OVERRIDE;
  virtual net::URLRequestContextGetter* GetMediaRequestContextForRenderProcess(
      int renderer_child_id) OVERRIDE;
  virtual net::URLRequestContextGetter*
      GetMediaRequestContextForStoragePartition(
          const base::FilePath& partition_path, bool in_memory) OVERRIDE;
  virtual content::ResourceContext* GetResourceContext() OVERRIDE;
  virtual content::DownloadManagerDelegate*
      GetDownloadManagerDelegate() OVERRIDE;
  virtual content::BrowserPluginGuestManager* GetGuestManager() OVERRIDE;
  virtual quota::SpecialStoragePolicy* GetSpecialStoragePolicy() OVERRIDE;
  virtual content::PushMessagingService* GetPushMessagingService() OVERRIDE;

  // visitedlink::VisitedLinkDelegate implementation.
  virtual void RebuildTable(
      const scoped_refptr<URLEnumerator>& enumerator) OVERRIDE;

 private:
  static bool data_reduction_proxy_enabled_;

  // The file path where data for this context is persisted.
  base::FilePath context_storage_path_;

  JniDependencyFactory* native_factory_;
  scoped_refptr<net::CookieStore> cookie_store_;
  scoped_refptr<AwURLRequestContextGetter> url_request_context_getter_;
  scoped_refptr<AwQuotaManagerBridge> quota_manager_bridge_;
  scoped_ptr<AwFormDatabaseService> form_database_service_;

  AwDownloadManagerDelegate download_manager_delegate_;

  scoped_ptr<visitedlink::VisitedLinkMaster> visitedlink_master_;
  scoped_ptr<content::ResourceContext> resource_context_;

  scoped_ptr<PrefService> user_pref_service_;

  scoped_ptr<DataReductionProxySettings> data_reduction_proxy_settings_;
  scoped_ptr<DataReductionProxyAuthRequestHandler>
      data_reduction_proxy_auth_request_handler_;

  DISALLOW_COPY_AND_ASSIGN(AwBrowserContext);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_BROWSER_CONTEXT_H_
