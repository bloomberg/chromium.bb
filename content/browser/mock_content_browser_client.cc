// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_content_browser_client.h"

#include <string>

#include "base/file_path.h"
#include "base/logging.h"
#include "content/test/test_web_contents_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

MockContentBrowserClient::MockContentBrowserClient() {
}

MockContentBrowserClient::~MockContentBrowserClient() {
}

BrowserMainParts* MockContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return NULL;
}

WebContentsView* MockContentBrowserClient::OverrideCreateWebContentsView(
    WebContents* web_contents) {
  return new TestWebContentsView;
}

WebContentsViewDelegate* MockContentBrowserClient::GetWebContentsViewDelegate(
    content::WebContents* web_contents) {
  return NULL;
}

void MockContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
}

void MockContentBrowserClient::RenderProcessHostCreated(
    RenderProcessHost* host) {
}

WebUIControllerFactory* MockContentBrowserClient::GetWebUIControllerFactory() {
  return NULL;
}

GURL MockContentBrowserClient::GetEffectiveURL(BrowserContext* browser_context,
                                               const GURL& url) {
  return url;
}

bool MockContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool MockContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool MockContentBrowserClient::IsSuitableHost(
    RenderProcessHost* process_host,
    const GURL& site_url) {
  return true;
}

bool MockContentBrowserClient::ShouldTryToUseExistingProcessHost(
    BrowserContext* browser_context, const GURL& url) {
  return false;
}

void MockContentBrowserClient::SiteInstanceGotProcess(
    SiteInstance* site_instance) {
}

void MockContentBrowserClient::SiteInstanceDeleting(
    SiteInstance* site_instance) {
}

bool MockContentBrowserClient::ShouldSwapProcessesForNavigation(
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

std::string MockContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

void MockContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
}

std::string MockContentBrowserClient::GetApplicationLocale() {
  return std::string();
}

std::string MockContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return std::string();
}

SkBitmap* MockContentBrowserClient::GetDefaultFavicon() {
  static SkBitmap empty;
  return &empty;
}

bool MockContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                             const GURL& first_party,
                                             ResourceContext* context) {
  return true;
}

bool MockContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    ResourceContext* context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool MockContentBrowserClient::AllowSetCookie(const GURL& url,
                                              const GURL& first_party,
                                              const std::string& cookie_line,
                                              ResourceContext* context,
                                              int render_process_id,
                                              int render_view_id,
                                              net::CookieOptions* options) {
  return true;
}

bool MockContentBrowserClient::AllowSaveLocalState(ResourceContext* context) {
  return true;
}

bool MockContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool MockContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool MockContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

QuotaPermissionContext*
    MockContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext* MockContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, ResourceContext* context) {
  return NULL;
}

void MockContentBrowserClient::OpenItem(const FilePath& path) {
}

void MockContentBrowserClient::ShowItemInFolder(const FilePath& path) {
}

void MockContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback,
    bool* cancel_request) {
}

void MockContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
}

void MockContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
}

void MockContentBrowserClient::RequestMediaAccessPermission(
    const MediaStreamRequest* request,
    const MediaResponseCallback& callback) {
}

MediaObserver* MockContentBrowserClient::GetMediaObserver() {
  return NULL;
}

void MockContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
}

WebKit::WebNotificationPresenter::Permission
    MockContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        ResourceContext* context,
        int render_process_id) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void MockContentBrowserClient::ShowDesktopNotification(
    const ShowDesktopNotificationHostMsgParams& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
}

void MockContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
}

bool MockContentBrowserClient::CanCreateWindow(
    const GURL& opener_url,
    const GURL& source_origin,
    WindowContainerType container_type,
    ResourceContext* context,
    int render_process_id,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

std::string MockContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, ResourceContext* context) {
  return std::string();
}

void MockContentBrowserClient::ResourceDispatcherHostCreated() {
}

SpeechRecognitionManagerDelegate*
    MockContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
  return NULL;
}

ui::Clipboard* MockContentBrowserClient::GetClipboard() {
  static ui::Clipboard clipboard;
  return &clipboard;
}

net::NetLog* MockContentBrowserClient::GetNetLog() {
  return NULL;
}

AccessTokenStore* MockContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool MockContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

void MockContentBrowserClient::OverrideWebkitPrefs(
    RenderViewHost* rvh,
    const GURL& url,
    webkit_glue::WebPreferences* prefs) {
}

void MockContentBrowserClient::UpdateInspectorSetting(
    RenderViewHost* rvh, const std::string& key, const std::string& value) {
}

void MockContentBrowserClient::ClearInspectorSettings(RenderViewHost* rvh) {
}

void MockContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
}

void MockContentBrowserClient::ClearCache(RenderViewHost* rvh) {
}

void MockContentBrowserClient::ClearCookies(RenderViewHost* rvh) {
}

FilePath MockContentBrowserClient::GetDefaultDownloadDirectory() {
  if (!download_dir_.IsValid()) {
    bool result = download_dir_.CreateUniqueTempDir();
    CHECK(result);
  }
  return download_dir_.path();
}

std::string MockContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
}

bool MockContentBrowserClient::AllowSocketAPI(BrowserContext* browser_context,
                                              const GURL& url) {
  return false;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int MockContentBrowserClient::GetCrashSignalFD(
    const CommandLine& command_line) {
  return -1;
}
#endif

#if defined(OS_WIN)
const wchar_t* MockContentBrowserClient::GetResourceDllName() {
  return NULL;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    MockContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

}  // namespace content
