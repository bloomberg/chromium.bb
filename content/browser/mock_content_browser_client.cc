// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mock_content_browser_client.h"

#include <string>

#include "base/file_path.h"
#include "content/browser/webui/empty_web_ui_factory.h"
#include "googleurl/src/gurl.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "webkit/glue/webpreferences.h"

namespace content {

MockContentBrowserClient::~MockContentBrowserClient() {
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

void MockContentBrowserClient::WorkerProcessHostCreated(
    WorkerProcessHost* host) {
}

WebUIFactory* MockContentBrowserClient::GetWebUIFactory() {
  // Return an empty factory so callsites don't have to check for NULL.
  return EmptyWebUIFactory::Get();
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
    const GURL& manifest_url, const content::ResourceContext& context) {
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
    Callback2<SSLCertErrorHandler*, bool>::Type* callback) {
}

void MockContentBrowserClient::ShowClientCertificateRequestDialog(
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
        const GURL& source_url,
        const content::ResourceContext& context) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void MockContentBrowserClient::ShowDesktopNotification(
    const DesktopNotificationHostMsg_Show_Params& params,
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
    const GURL& source_url,
    WindowContainerType container_type,
    const content::ResourceContext& context) {
  return false;
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

bool MockContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

WebPreferences MockContentBrowserClient::GetWebkitPrefs(
    content::BrowserContext* browser_context,
    bool is_web_ui) {
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
  return FilePath();
}

net::URLRequestContextGetter*
MockContentBrowserClient::GetDefaultRequestContextDeprecatedCrBug64339() {
  return NULL;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int MockContentBrowserClient::GetCrashSignalFD(
    const std::string& process_type) {
  return -1;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    MockContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

}  // namespace content
