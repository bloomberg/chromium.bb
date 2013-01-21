// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_content_browser_client.h"

#include "android_webview/browser/aw_browser_main_parts.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/aw_quota_permission_context.h"
#include "android_webview/browser/net_disk_cache_remover.h"
#include "android_webview/browser/renderer_host/aw_resource_dispatcher_host_delegate.h"
#include "android_webview/common/url_constants.h"
#include "base/android/locale_utils.h"
#include "base/base_paths_android.h"
#include "base/path_service.h"
#include "content/public/browser/access_token_store.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/url_constants.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

class DummyAccessTokenStore : public content::AccessTokenStore {
 public:
  DummyAccessTokenStore() { }

  virtual void LoadAccessTokens(
      const LoadAccessTokensCallbackType& request) OVERRIDE { }

 private:
  virtual ~DummyAccessTokenStore() { }

  virtual void SaveAccessToken(
      const GURL& server_url, const string16& access_token) OVERRIDE { }

  DISALLOW_COPY_AND_ASSIGN(DummyAccessTokenStore);
};

}

namespace android_webview {

AwContentBrowserClient::AwContentBrowserClient(
    ViewDelegateFactoryFn* view_delegate_factory,
    GeolocationPermissionFactoryFn* geolocation_permission_factory)
    : view_delegate_factory_(view_delegate_factory) {
  FilePath user_data_dir;
  if (!PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir)) {
    NOTREACHED() << "Failed to get app data directory for Android WebView";
  }
  browser_context_.reset(
      new AwBrowserContext(user_data_dir, geolocation_permission_factory));
}

AwContentBrowserClient::~AwContentBrowserClient() {
}

AwBrowserContext* AwContentBrowserClient::GetAwBrowserContext() {
  return browser_context_.get();
}

content::BrowserMainParts* AwContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  return new AwBrowserMainParts(browser_context_.get());
}

content::WebContentsViewDelegate*
AwContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return (*view_delegate_factory_)(web_contents);
}

void AwContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
  // If WebView becomes multi-process capable, this may be insecure.
  // More benefit can be derived from the ChildProcessSecurotyPolicy by
  // deferring the GrantScheme calls until we know that a given child process
  // really does need that priviledge. Check here to ensure we rethink this
  // when the time comes. See crbug.com/156062.
  CHECK(content::RenderProcessHost::run_renderer_in_process());

  // Grant content: and file: scheme to the whole process, since we impose
  // per-view access checks.
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), android_webview::kContentScheme);
  content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
      host->GetID(), chrome::kFileScheme);
}

std::string AwContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return alias_name;
}

void AwContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line,
    int child_process_id) {
  NOTREACHED() << "Android WebView does not support multi-process yet";
}

std::string AwContentBrowserClient::GetApplicationLocale() {
  return base::android::GetDefaultLocale();
}

std::string AwContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  // Start with the currnet locale.
  std::string langs = GetApplicationLocale();

  // If we're not en-US, add in en-US which will be
  // used with a lower q-value.
  if (StringToLowerASCII(langs) != "en-us") {
    langs += ",en-US";
  }
  return langs;
}

gfx::ImageSkia* AwContentBrowserClient::GetDefaultFavicon() {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  // TODO(boliu): Bundle our own default favicon?
  return rb.GetImageSkiaNamed(IDR_DEFAULT_FAVICON);
}

bool AwContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                           const GURL& first_party,
                           content::ResourceContext* context) {
  // WebView doesn't have a per-site policy for locally stored data,
  // instead AppCache can be disabled for individual WebViews.
  return true;
}


bool AwContentBrowserClient::AllowGetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const net::CookieList& cookie_list,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_view_id) {
  return AwCookieAccessPolicy::GetInstance()->AllowGetCookie(url,
                                                             first_party,
                                                             cookie_list,
                                                             context,
                                                             render_process_id,
                                                             render_view_id);
}

bool AwContentBrowserClient::AllowSetCookie(const GURL& url,
                                            const GURL& first_party,
                                            const std::string& cookie_line,
                                            content::ResourceContext* context,
                                            int render_process_id,
                                            int render_view_id,
                                            net::CookieOptions* options) {
  return AwCookieAccessPolicy::GetInstance()->AllowSetCookie(url,
                                                             first_party,
                                                             cookie_line,
                                                             context,
                                                             render_process_id,
                                                             render_view_id,
                                                             options);
}

bool AwContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  // Android WebView does not yet support web workers.
  return false;
}

bool AwContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  // Android WebView does not yet support web workers.
  return false;
}

bool AwContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  // Android WebView does not yet support web workers.
  return false;
}

content::QuotaPermissionContext*
AwContentBrowserClient::CreateQuotaPermissionContext() {
  return new AwQuotaPermissionContext;
}

void AwContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    bool strict_enforcement,
    const base::Callback<void(bool)>& callback,
    bool* cancel_request) {
  // TODO(boliu): Implement this to power WebViewClient.onReceivedSslError.
  NOTIMPLEMENTED();
  *cancel_request = true;
}

WebKit::WebNotificationPresenter::Permission
    AwContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        content::ResourceContext* context,
        int render_process_id) {
  // Android WebView does not support notifications, so return Denied here.
  return WebKit::WebNotificationPresenter::PermissionDenied;
}

void AwContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
  NOTREACHED() << "Android WebView does not support desktop notifications.";
}

void AwContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
  NOTREACHED() << "Android WebView does not support desktop notifications.";
}

bool AwContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    content::ResourceContext* context,
    int render_process_id,
    bool* no_javascript_access) {
  // We unconditionally allow popup windows at this stage and will give
  // the embedder the opporunity to handle displaying of the popup in
  // WebContentsDelegate::AddContents (via the
  // AwContentsClient.onCreateWindow callback).
  // Note that if the embedder has blocked support for creating popup
  // windows through AwSettings, then we won't get to this point as
  // the popup creation will have been blocked at the WebKit level.
  if (no_javascript_access) {
    *no_javascript_access = false;
  }
  return true;
}

std::string AwContentBrowserClient::GetWorkerProcessTitle(const GURL& url,
                                          content::ResourceContext* context) {
  NOTREACHED() << "Android WebView does not yet support web workers.";
  return std::string();
}


void AwContentBrowserClient::ResourceDispatcherHostCreated() {
  AwResourceDispatcherHostDelegate::ResourceDispatcherHostCreated();
}

net::NetLog* AwContentBrowserClient::GetNetLog() {
  // TODO(boliu): Implement AwNetLog.
  return NULL;
}

content::AccessTokenStore* AwContentBrowserClient::CreateAccessTokenStore() {
  // TODO(boliu): Implement as part of geolocation code.
  return new DummyAccessTokenStore();
}

bool AwContentBrowserClient::IsFastShutdownPossible() {
  NOTREACHED() << "Android WebView is single process, so IsFastShutdownPossible"
               << " should never be called";
  return false;
}

void AwContentBrowserClient::UpdateInspectorSetting(
    content::RenderViewHost* rvh,
    const std::string& key,
    const std::string& value) {
  // TODO(boliu): Implement persisting inspector settings.
  NOTIMPLEMENTED();
}

void AwContentBrowserClient::ClearInspectorSettings(
    content::RenderViewHost* rvh) {
  // TODO(boliu): Implement persisting inspector settings.
  NOTIMPLEMENTED();
}

void AwContentBrowserClient::ClearCache(content::RenderViewHost* rvh) {
  RemoveHttpDiskCache(rvh->GetProcess()->GetBrowserContext(),
                      rvh->GetProcess()->GetID());
}

void AwContentBrowserClient::ClearCookies(content::RenderViewHost* rvh) {
  // TODO(boliu): Implement.
  NOTIMPLEMENTED();
}

FilePath AwContentBrowserClient::GetDefaultDownloadDirectory() {
  // Android WebView does not currently use the Chromium downloads system.
  // Download requests are cancelled immedately when recognized; see
  // AwResourceDispatcherHost::CreateResourceHandlerForDownload. However the
  // download system still tries to start up and calls this before recognizing
  // the request has been cancelled.
  return FilePath();
}

std::string AwContentBrowserClient::GetDefaultDownloadName() {
  NOTREACHED() << "Android WebView does not use chromium downloads";
  return std::string();
}

void AwContentBrowserClient::DidCreatePpapiPlugin(
    content::BrowserPpapiHost* browser_host) {
  NOTREACHED() << "Android WebView does not support plugins";
}

bool AwContentBrowserClient::AllowPepperSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url,
    const content::SocketPermissionRequest& params) {
  NOTREACHED() << "Android WebView does not support plugins";
  return false;
}

}  // namespace android_webview
