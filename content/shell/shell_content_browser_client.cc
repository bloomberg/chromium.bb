// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_content_browser_client.h"

#include "base/file_path.h"
#include "content/browser/webui/empty_web_ui_factory.h"
#include "content/shell/shell.h"
#include "content/shell/shell_browser_main.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/webpreferences.h"

#if defined(OS_WIN)
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view_win.h"
#include "content/common/view_messages.h"
#endif

namespace content {

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(NULL) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return new ShellBrowserMainParts(parameters);
}

TabContentsView* ShellContentBrowserClient::CreateTabContentsView(
    TabContents* tab_contents) {
#if defined(OS_WIN)
  return new TabContentsViewWin(tab_contents, this);
#else
  return NULL;
#endif
}

void ShellContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
}

void ShellContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
}

void ShellContentBrowserClient::PluginProcessHostCreated(
    PluginProcessHost* host) {
}

void ShellContentBrowserClient::WorkerProcessHostCreated(
    WorkerProcessHost* host) {
}

WebUIFactory* ShellContentBrowserClient::GetWebUIFactory() {
  // Return an empty factory so callsites don't have to check for NULL.
  return EmptyWebUIFactory::GetInstance();
}

GURL ShellContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  return GURL();
}

bool ShellContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool ShellContentBrowserClient::IsURLSameAsAnySiteInstance(const GURL& url) {
  return false;
}

bool ShellContentBrowserClient::IsSuitableHost(
    RenderProcessHost* process_host,
    const GURL& site_url) {
  return true;
}

std::string ShellContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
}

std::string ShellContentBrowserClient::GetApplicationLocale() {
  return std::string();
}

std::string ShellContentBrowserClient::GetAcceptLangs(const TabContents* tab) {
  return std::string();
}

SkBitmap* ShellContentBrowserClient::GetDefaultFavicon() {
  static SkBitmap empty;
  return &empty;
}

bool ShellContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    const content::ResourceContext& context) {
  return true;
}

bool ShellContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool ShellContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  return true;
}

bool ShellContentBrowserClient::AllowSaveLocalState(
    const content::ResourceContext& context) {
  return true;
}

QuotaPermissionContext*
    ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext* ShellContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, const content::ResourceContext& context) {
  return NULL;
}

void ShellContentBrowserClient::OpenItem(const FilePath& path) {
}

void ShellContentBrowserClient::ShowItemInFolder(const FilePath& path) {
}

void ShellContentBrowserClient::AllowCertificateError(
    SSLCertErrorHandler* handler,
    bool overridable,
    Callback2<SSLCertErrorHandler*, bool>::Type* callback) {
}

void ShellContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    SSLClientAuthHandler* handler) {
}

void ShellContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
}

void ShellContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
}

WebKit::WebNotificationPresenter::Permission
    ShellContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        const content::ResourceContext& context) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void ShellContentBrowserClient::ShowDesktopNotification(
    const DesktopNotificationHostMsg_Show_Params& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
}

void ShellContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
}

bool ShellContentBrowserClient::CanCreateWindow(
    const GURL& source_url,
    WindowContainerType container_type,
    const content::ResourceContext& context) {
  return true;
}

std::string ShellContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, const content::ResourceContext& context) {
  return std::string();
}

ResourceDispatcherHost* ShellContentBrowserClient::GetResourceDispatcherHost() {
  return shell_browser_main_parts_->GetResourceDispatcherHost();
}

ui::Clipboard* ShellContentBrowserClient::GetClipboard() {
  return shell_browser_main_parts_->GetClipboard();
}

MHTMLGenerationManager* ShellContentBrowserClient::GetMHTMLGenerationManager() {
  return NULL;
}

DevToolsManager* ShellContentBrowserClient::GetDevToolsManager() {
  return NULL;
}

net::NetLog* ShellContentBrowserClient::GetNetLog() {
  return NULL;
}

speech_input::SpeechInputManager*
    ShellContentBrowserClient::GetSpeechInputManager() {
  return NULL;
}

AccessTokenStore* ShellContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool ShellContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

WebPreferences ShellContentBrowserClient::GetWebkitPrefs(
    content::BrowserContext* browser_context,
    bool is_web_ui) {
  return WebPreferences();
}

void ShellContentBrowserClient::UpdateInspectorSetting(
    RenderViewHost* rvh, const std::string& key, const std::string& value) {
}

void ShellContentBrowserClient::ClearInspectorSettings(RenderViewHost* rvh) {
}

void ShellContentBrowserClient::BrowserURLHandlerCreated(
    BrowserURLHandler* handler) {
}

void ShellContentBrowserClient::ClearCache(RenderViewHost* rvh) {
}

void ShellContentBrowserClient::ClearCookies(RenderViewHost* rvh) {
}

FilePath ShellContentBrowserClient::GetDefaultDownloadDirectory() {
  return FilePath();
}

net::URLRequestContextGetter*
ShellContentBrowserClient::GetDefaultRequestContextDeprecatedCrBug64339() {
  return NULL;
}

net::URLRequestContextGetter*
ShellContentBrowserClient::GetSystemRequestContext() {
  return NULL;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int ShellContentBrowserClient::GetCrashSignalFD(
    const std::string& process_type) {
  return -1;
}
#endif

#if defined(OS_WIN)
const wchar_t* ShellContentBrowserClient::GetResourceDllName() {
  return NULL;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ShellContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

#if defined(OS_WIN)
TabContents* ShellContentBrowserClient::CreateNewWindow(
    TabContentsViewWin* tab_contents_view,
    int route_id,
    const ViewHostMsg_CreateWindow_Params& params) {
  Shell* shell = Shell::CreateNewWindow(
      tab_contents_view->tab_contents()->browser_context(),
      GURL(),
      tab_contents_view->tab_contents()->GetSiteInstance(),
      route_id,
      tab_contents_view->tab_contents());
  return shell->tab_contents();
}
#endif

}  // namespace content
