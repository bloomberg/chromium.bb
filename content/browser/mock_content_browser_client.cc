// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_content_browser_client.h"

#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "content/browser/webui/empty_web_ui_factory.h"
#include "content/test/test_tab_contents_view.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "webkit/glue/webpreferences.h"

namespace content {

MockContentBrowserClient::MockContentBrowserClient() {
}

MockContentBrowserClient::~MockContentBrowserClient() {
}

void MockContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters,
    std::vector<BrowserMainParts*>* parts_list) {
}

RenderWidgetHostView* MockContentBrowserClient::CreateViewForWidget(
    RenderWidgetHost* widget) {
  return NULL;
}

TabContentsView* MockContentBrowserClient::CreateTabContentsView(
    TabContents* tab_contents) {
  return new TestTabContentsView;
}

void MockContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
}

void MockContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
}

void MockContentBrowserClient::PluginProcessHostCreated(
    PluginProcessHost* host) {
}

WebUIFactory* MockContentBrowserClient::GetWebUIFactory() {
  // Return an empty factory so callsites don't have to check for NULL.
  return EmptyWebUIFactory::GetInstance();
}

GURL MockContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  return GURL();
}

bool MockContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool MockContentBrowserClient::IsURLSameAsAnySiteInstance(const GURL& url) {
  return false;
}

bool MockContentBrowserClient::IsSuitableHost(
    RenderProcessHost* process_host,
    const GURL& site_url) {
  return true;
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

std::string MockContentBrowserClient::GetAcceptLangs(const TabContents* tab) {
  return std::string();
}

SkBitmap* MockContentBrowserClient::GetDefaultFavicon() {
  static SkBitmap empty;
  return &empty;
}

bool MockContentBrowserClient::AllowAppCache(
    const GURL& manifest_url, const GURL& first_party,
    const content::ResourceContext& context) {
  return true;
}

bool MockContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool MockContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  return true;
}

bool MockContentBrowserClient::AllowSaveLocalState(
    const content::ResourceContext& context) {
  return true;
}

bool MockContentBrowserClient::AllowWorkerDatabase(
    int worker_route_id,
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    WorkerProcessHost* worker_process_host) {
  return true;
}

bool MockContentBrowserClient::AllowWorkerFileSystem(
    int worker_route_id,
    const GURL& url,
    WorkerProcessHost* worker_process_host) {
  return true;
}

QuotaPermissionContext*
    MockContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext* MockContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, const content::ResourceContext& context) {
  return NULL;
}

void MockContentBrowserClient::OpenItem(const FilePath& path) {
}

void MockContentBrowserClient::ShowItemInFolder(const FilePath& path) {
}

void MockContentBrowserClient::AllowCertificateError(
    SSLCertErrorHandler* handler,
    bool overridable,
    const base::Callback<void(SSLCertErrorHandler*, bool)>& callback) {
}

void MockContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    SSLClientAuthHandler* handler) {
}

void MockContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
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
        const content::ResourceContext& context,
        int render_process_id) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void MockContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
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
    const GURL& source_origin,
    WindowContainerType container_type,
    const content::ResourceContext& context,
    int render_process_id) {
  return true;
}

std::string MockContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, const content::ResourceContext& context) {
  return std::string();
}

ResourceDispatcherHost* MockContentBrowserClient::GetResourceDispatcherHost() {
  return NULL;
}

ui::Clipboard* MockContentBrowserClient::GetClipboard() {
  static ui::Clipboard clipboard;
  return &clipboard;
}

MHTMLGenerationManager* MockContentBrowserClient::GetMHTMLGenerationManager() {
  return NULL;
}

DevToolsManager* MockContentBrowserClient::GetDevToolsManager() {
  return NULL;
}

net::NetLog* MockContentBrowserClient::GetNetLog() {
  return NULL;
}

speech_input::SpeechInputManager*
    MockContentBrowserClient::GetSpeechInputManager() {
  return NULL;
}

AccessTokenStore* MockContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool MockContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

WebPreferences MockContentBrowserClient::GetWebkitPrefs(RenderViewHost* rvh) {
  return WebPreferences();
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
