// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_content_browser_client.h"

#include "ash/shell/content_client/shell_browser_main_parts.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/shell/shell.h"
#include "content/shell/shell_devtools_delegate.h"
#include "content/shell/shell_render_view_host_observer.h"
#include "content/shell/shell_resource_dispatcher_host_delegate.h"
#include "content/shell/shell_switches.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(NULL) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  shell_browser_main_parts_ =  new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

content::WebContentsView*
    ShellContentBrowserClient::OverrideCreateWebContentsView(
        content::WebContents* web_contents) {
  return NULL;
}

content::WebContentsViewDelegate*
    ShellContentBrowserClient::GetWebContentsViewDelegate(
        content::WebContents* web_contents) {
  return NULL;
}

void ShellContentBrowserClient::RenderViewHostCreated(
    content::RenderViewHost* render_view_host) {
  new content::ShellRenderViewHostObserver(render_view_host);
}

void ShellContentBrowserClient::RenderProcessHostCreated(
    content::RenderProcessHost* host) {
}

content::WebUIControllerFactory*
    ShellContentBrowserClient::GetWebUIControllerFactory() {
  return NULL;
}

GURL ShellContentBrowserClient::GetEffectiveURL(
    content::BrowserContext* browser_context, const GURL& url) {
  return GURL();
}

bool ShellContentBrowserClient::ShouldUseProcessPerSite(
    content::BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

bool ShellContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ShellContentBrowserClient::IsSuitableHost(
    content::RenderProcessHost* process_host,
    const GURL& site_url) {
  return true;
}

bool ShellContentBrowserClient::ShouldTryToUseExistingProcessHost(
    content::BrowserContext* browser_context, const GURL& url) {
  return false;
}

void ShellContentBrowserClient::SiteInstanceGotProcess(
    content::SiteInstance* site_instance) {
}

void ShellContentBrowserClient::SiteInstanceDeleting(
    content::SiteInstance* site_instance) {
}

bool ShellContentBrowserClient::ShouldSwapProcessesForNavigation(
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

std::string ShellContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

void ShellContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kDumpRenderTree))
    command_line->AppendSwitch(switches::kDumpRenderTree);
}

std::string ShellContentBrowserClient::GetApplicationLocale() {
  return std::string();
}

std::string ShellContentBrowserClient::GetAcceptLangs(
    content::BrowserContext* context) {
  return std::string();
}

SkBitmap* ShellContentBrowserClient::GetDefaultFavicon() {
  static SkBitmap empty;
  return &empty;
}

bool ShellContentBrowserClient::AllowAppCache(
    const GURL& manifest_url,
    const GURL& first_party,
    content::ResourceContext* context) {
  return true;
}

bool ShellContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    content::ResourceContext* context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool ShellContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    content::ResourceContext* context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  return true;
}

bool ShellContentBrowserClient::AllowSaveLocalState(
    content::ResourceContext* context) {
  return true;
}

bool ShellContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ShellContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ShellContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    content::ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

content::QuotaPermissionContext*
    ShellContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext*
    ShellContentBrowserClient::OverrideRequestContextForURL(
        const GURL& url, content::ResourceContext* context) {
  return NULL;
}

void ShellContentBrowserClient::OpenItem(const FilePath& path) {
}

void ShellContentBrowserClient::ShowItemInFolder(const FilePath& path) {
}

void ShellContentBrowserClient::AllowCertificateError(
    int render_process_id,
    int render_view_id,
    int cert_error,
    const net::SSLInfo& ssl_info,
    const GURL& request_url,
    bool overridable,
    const base::Callback<void(bool)>& callback,
    bool* cancel_request) {
}

void ShellContentBrowserClient::SelectClientCertificate(
    int render_process_id,
    int render_view_id,
    const net::HttpNetworkSession* network_session,
    net::SSLCertRequestInfo* cert_request_info,
    const base::Callback<void(net::X509Certificate*)>& callback) {
}

void ShellContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
}

void ShellContentBrowserClient::RequestMediaAccessPermission(
    const content::MediaStreamRequest* request,
    const content::MediaResponseCallback& callback) {
}

content::MediaObserver* ShellContentBrowserClient::GetMediaObserver() {
  return NULL;
}

void ShellContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
}

WebKit::WebNotificationPresenter::Permission
    ShellContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        content::ResourceContext* context,
        int render_process_id) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void ShellContentBrowserClient::ShowDesktopNotification(
    const content::ShowDesktopNotificationHostMsgParams& params,
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
    const GURL& opener_url,
    const GURL& origin,
    WindowContainerType container_type,
    content::ResourceContext* context,
    int render_process_id,
    bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

std::string ShellContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, content::ResourceContext* context) {
  return std::string();
}

void ShellContentBrowserClient::ResourceDispatcherHostCreated() {
  resource_dispatcher_host_delegate_.reset(
      new content::ShellResourceDispatcherHostDelegate);
  content::ResourceDispatcherHost::Get()->SetDelegate(
      resource_dispatcher_host_delegate_.get());
}

content::SpeechRecognitionManagerDelegate*
    ShellContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
  return NULL;
}

ui::Clipboard* ShellContentBrowserClient::GetClipboard() {
  return shell_browser_main_parts_->GetClipboard();
}

net::NetLog* ShellContentBrowserClient::GetNetLog() {
  return NULL;
}

content::AccessTokenStore*
    ShellContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool ShellContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

void ShellContentBrowserClient::OverrideWebkitPrefs(
    content::RenderViewHost* rvh,
    const GURL& url,
    webkit_glue::WebPreferences* prefs) {
}

void ShellContentBrowserClient::UpdateInspectorSetting(
    content::RenderViewHost* rvh,
    const std::string& key,
    const std::string& value) {
}

void ShellContentBrowserClient::ClearInspectorSettings(
    content::RenderViewHost* rvh) {
}

void ShellContentBrowserClient::BrowserURLHandlerCreated(
    content::BrowserURLHandler* handler) {
}

void ShellContentBrowserClient::ClearCache(content::RenderViewHost* rvh) {
}

void ShellContentBrowserClient::ClearCookies(content::RenderViewHost* rvh) {
}

FilePath ShellContentBrowserClient::GetDefaultDownloadDirectory() {
  return FilePath();
}

std::string ShellContentBrowserClient::GetDefaultDownloadName() {
  return "download";
}

bool ShellContentBrowserClient::AllowSocketAPI(
    content::BrowserContext* browser_context,
    const GURL& url) {
  return false;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int ShellContentBrowserClient::GetCrashSignalFD(
    const CommandLine& command_line) {
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

content::ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

}  // namespace examples
}  // namespace views
