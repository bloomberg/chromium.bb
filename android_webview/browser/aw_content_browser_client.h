// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
#define ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_

#include "android_webview/browser/aw_web_preferences_populater.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/content_browser_client.h"

namespace android_webview {

class AwBrowserContext;
class JniDependencyFactory;

class AwContentBrowserClient : public content::ContentBrowserClient {
 public:
  // This is what AwContentBrowserClient::GetAcceptLangs uses.
  static std::string GetAcceptLangsImpl();

  // Deprecated: use AwBrowserContext::GetDefault() instead.
  static AwBrowserContext* GetAwBrowserContext();

  AwContentBrowserClient(JniDependencyFactory* native_factory);
  virtual ~AwContentBrowserClient();

  // Overriden methods from ContentBrowserClient.
  virtual void AddCertificate(net::CertificateMimeType cert_type,
                              const void* cert_data,
                              size_t cert_size,
                              int render_process_id,
                              int render_frame_id) override;
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) override;
  virtual content::WebContentsViewDelegate* GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  virtual void RenderProcessWillLaunch(
      content::RenderProcessHost* host) override;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual net::URLRequestContextGetter* CreateRequestContextForStoragePartition(
      content::BrowserContext* browser_context,
      const base::FilePath& partition_path,
      bool in_memory,
      content::ProtocolHandlerMap* protocol_handlers,
      content::URLRequestInterceptorScopedVector request_interceptors) override;
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name) override;
  virtual void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                              int child_process_id) override;
  virtual std::string GetApplicationLocale() override;
  virtual std::string GetAcceptLangs(content::BrowserContext* context) override;
  virtual const gfx::ImageSkia* GetDefaultFavicon() override;
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const GURL& first_party,
                             content::ResourceContext* context) override;
  virtual bool AllowGetCookie(const GURL& url,
                              const GURL& first_party,
                              const net::CookieList& cookie_list,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id) override;
  virtual bool AllowSetCookie(const GURL& url,
                              const GURL& first_party,
                              const std::string& cookie_line,
                              content::ResourceContext* context,
                              int render_process_id,
                              int render_frame_id,
                              net::CookieOptions* options) override;
  virtual bool AllowWorkerDatabase(
      const GURL& url,
      const base::string16& name,
      const base::string16& display_name,
      unsigned long estimated_size,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) override;
  virtual void AllowWorkerFileSystem(
      const GURL& url,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames,
      base::Callback<void(bool)> callback) override;
  virtual bool AllowWorkerIndexedDB(
      const GURL& url,
      const base::string16& name,
      content::ResourceContext* context,
      const std::vector<std::pair<int, int> >& render_frames) override;
  virtual content::QuotaPermissionContext*
      CreateQuotaPermissionContext() override;
  virtual void AllowCertificateError(
      int render_process_id,
      int render_frame_id,
      int cert_error,
      const net::SSLInfo& ssl_info,
      const GURL& request_url,
      content::ResourceType resource_type,
      bool overridable,
      bool strict_enforcement,
      bool expired_previous_decision,
      const base::Callback<void(bool)>& callback,
      content::CertificateRequestResultType* result) override;
  virtual void SelectClientCertificate(
      int render_process_id,
      int render_frame_id,
      net::SSLCertRequestInfo* cert_request_info,
      const base::Callback<void(net::X509Certificate*)>& callback) override;
  virtual blink::WebNotificationPermission
      CheckDesktopNotificationPermission(
          const GURL& source_url,
          content::ResourceContext* context,
          int render_process_id) override;
  virtual void ShowDesktopNotification(
      const content::ShowDesktopNotificationHostMsgParams& params,
      content::BrowserContext* browser_context,
      int render_process_id,
      scoped_ptr<content::DesktopNotificationDelegate> delegate,
      base::Closure* cancel_callback) override;
  virtual void RequestPermission(
      content::PermissionType permission,
      content::WebContents* web_contents,
      int bridge_id,
      const GURL& requesting_frame,
      bool user_gesture,
      const base::Callback<void(bool)>& result_callback) override;
  virtual void CancelPermissionRequest(content::PermissionType permission,
                                       content::WebContents* web_contents,
                                       int bridge_id,
                                       const GURL& origin) override;
  virtual bool CanCreateWindow(const GURL& opener_url,
                               const GURL& opener_top_level_frame_url,
                               const GURL& source_origin,
                               WindowContainerType container_type,
                               const GURL& target_url,
                               const content::Referrer& referrer,
                               WindowOpenDisposition disposition,
                               const blink::WebWindowFeatures& features,
                               bool user_gesture,
                               bool opener_suppressed,
                               content::ResourceContext* context,
                               int render_process_id,
                               int opener_id,
                               bool* no_javascript_access) override;
  virtual void ResourceDispatcherHostCreated() override;
  virtual net::NetLog* GetNetLog() override;
  virtual content::AccessTokenStore* CreateAccessTokenStore() override;
  virtual bool IsFastShutdownPossible() override;
  virtual void ClearCache(content::RenderViewHost* rvh) override;
  virtual void ClearCookies(content::RenderViewHost* rvh) override;
  virtual base::FilePath GetDefaultDownloadDirectory() override;
  virtual std::string GetDefaultDownloadName() override;
  virtual void DidCreatePpapiPlugin(
      content::BrowserPpapiHost* browser_host) override;
  virtual bool AllowPepperSocketAPI(
      content::BrowserContext* browser_context,
      const GURL& url,
      bool private_api,
      const content::SocketPermissionRequest* params) override;
  virtual void OverrideWebkitPrefs(content::RenderViewHost* rvh,
                                   const GURL& url,
                                   content::WebPreferences* web_prefs) override;
#if defined(VIDEO_HOLE)
  virtual content::ExternalVideoSurfaceContainer*
      OverrideCreateExternalVideoSurfaceContainer(
          content::WebContents* web_contents) override;
#endif
  virtual content::DevToolsManagerDelegate*
      GetDevToolsManagerDelegate() override;

 private:
  // Android WebView currently has a single global (non-off-the-record) browser
  // context.
  scoped_ptr<AwBrowserContext> browser_context_;
  scoped_ptr<AwWebPreferencesPopulater> preferences_populater_;

  JniDependencyFactory* native_factory_;

  DISALLOW_COPY_AND_ASSIGN(AwContentBrowserClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_LIB_AW_CONTENT_BROWSER_CLIENT_H_
