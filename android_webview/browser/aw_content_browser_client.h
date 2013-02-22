// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
#define ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"
#include "net/url_request/url_request_job_factory.h"

namespace android_webview {

class AwBrowserContext;
class JniDependencyFactory;

class AwContentBrowserClient : public content::ContentBrowserClient {
 public:
  static AwContentBrowserClient* FromContentBrowserClient(
      content::ContentBrowserClient* client);

  AwContentBrowserClient(JniDependencyFactory* native_factory);
  virtual ~AwContentBrowserClient();

  AwBrowserContext* GetAwBrowserContext();

  // Overriden methods from ContentBrowserClient.
  virtual void AddCertificate(net::URLRequest* request,
                              net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_view_id) OVERRIDE;
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) OVERRIDE;
  virtual void RenderProcessHostCreated(
      content::RenderProcessHost* host) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          blob_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          file_system_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          developer_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_devtools_protocol_handler) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          blob_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          file_system_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          developer_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_protocol_handler,
      scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
          chrome_devtools_protocol_handler) OVERRIDE;
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) OVERRIDE;
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id) OVERRIDE;
  virtual std::string GetApplicationLocale() OVERRIDE;
  virtual std::string GetAcceptLangs(content::BrowserContext* context) OVERRIDE;
  virtual gfx::ImageSkia* GetDefaultFavicon() OVERRIDE;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) OVERRIDE;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id) OVERRIDE;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_view_id,
                              net::CookieOptions* options) OVERRIDE;
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const string16& name,
      const string16& display_name,
      unsigned long estimated_size,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual bool AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_views) OVERRIDE;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() OVERRIDE;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_view_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      bool overridable,
      bool strict_enforcement,
      const base::Callback<void(bool)>& callback,
      bool* cancel_request) OVERRIDE;
  virtual WebKit::WebNotificationPresenter::Permission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          content::ResourceContext* context,
          int render_process_id) OVERRIDE;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      int render_process_id,
      int render_view_id,
      bool worker) OVERRIDE;
  virtual void CancelDesktopNotification(
      int render_process_id,
      int render_view_id,
      int notification_id) OVERRIDE;
  virtual bool CanCreateWindow(
      const GURL& opener_url,
      const GURL& source_origin,
      WindowContainerType container_type,
      content::ResourceContext* context,
      int render_process_id,
      bool* no_javascript_access) OVERRIDE;
  virtual std::string GetWorkerProcessTitle(
      const GURL& url,
      content::ResourceContext* context) OVERRIDE;
  virtual void ResourceDispatcherHostCreated() OVERRIDE;
  virtual net::NetLog* GetNetLog() OVERRIDE;
  virtual content::AccessTokenStore* CreateAccessTokenStore() OVERRIDE;
  virtual bool IsFastShutdownPossible() OVERRIDE;
  virtual void UpdateInspectorSetting(content::RenderViewHost* rvh,
                                      const std::string& key,
                                      const std::string& value) OVERRIDE;
  virtual void ClearInspectorSettings(content::RenderViewHost* rvh) OVERRIDE;
  virtual void ClearCache(content::RenderViewHost* rvh) OVERRIDE;
  virtual void ClearCookies(content::RenderViewHost* rvh) OVERRIDE;
  virtual base::FilePath GetDefaultDownloadDirectory() OVERRIDE;
  virtual std::string GetDefaultDownloadName() OVERRIDE;
  virtual void DidCreatePpapiPlugin(
      content::BrowserPpapiHost* browser_host) OVERRIDE;
  virtual bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      const content::SocketPermissionRequest& params) OVERRIDE;

 private:
  // Android WebView currently has a single global (non-off-the-record) browser
  // context.
  scoped_ptr<AwBrowserContext> browser_context_;

  JniDependencyFactory* native_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwContentBrowserClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
