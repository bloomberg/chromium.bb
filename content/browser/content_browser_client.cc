// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/content_browser_client.h"

#include "base/memory/singleton.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "content/browser/ssl/ssl_client_auth_handler.h"
#include "content/browser/webui/empty_web_ui_factory.h"
#include "googleurl/src/gurl.h"
#include "ui/base/clipboard/clipboard.h"

namespace content {

void ContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
}

void ContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
}

void ContentBrowserClient::PluginProcessHostCreated(PluginProcessHost* host) {
}

void ContentBrowserClient::WorkerProcessHostCreated(WorkerProcessHost* host) {
}

WebUIFactory* ContentBrowserClient::GetWebUIFactory() {
  // Return an empty factory so callsites don't have to check for NULL.
  return EmptyWebUIFactory::Get();
}

GURL ContentBrowserClient::GetEffectiveURL(Profile* profile, const GURL& url) {
  return url;
}

bool ContentBrowserClient::IsURLSameAsAnySiteInstance(const GURL& url) {
  return false;
}

std::string ContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return alias_name;
}

void ContentBrowserClient::AppendExtraCommandLineSwitches(
    CommandLine* command_line, int child_process_id) {
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return std::string();
}

std::string ContentBrowserClient::GetAcceptLangs(const TabContents* tab) {
  return std::string();
}

bool ContentBrowserClient::AllowAppCache(
    const GURL& manifest_url, const content::ResourceContext& context) {
  return true;
}

bool ContentBrowserClient::AllowGetCookie(
    const GURL& url,
    const GURL& first_party,
    const net::CookieList& cookie_list,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id) {
  return true;
}

bool ContentBrowserClient::AllowSetCookie(
    const GURL& url,
    const GURL& first_party,
    const std::string& cookie_line,
    const content::ResourceContext& context,
    int render_process_id,
    int render_view_id,
    net::CookieOptions* options) {
  return true;
}

QuotaPermissionContext* ContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

void ContentBrowserClient::RevealFolderInOS(const FilePath& path) {
}

void ContentBrowserClient::AllowCertificateError(
    SSLCertErrorHandler* handler,
    bool overridable,
    Callback2<SSLCertErrorHandler*, bool>::Type* callback) {
  callback->Run(handler, overridable);
  delete callback;
}

void ContentBrowserClient::ShowClientCertificateRequestDialog(
    int render_process_id,
    int render_view_id,
    SSLClientAuthHandler* handler) {
  handler->CertificateSelected(NULL);
}

void ContentBrowserClient::AddNewCertificate(
    net::URLRequest* request,
    net::X509Certificate* cert,
    int render_process_id,
    int render_view_id) {
}

void ContentBrowserClient::RequestDesktopNotificationPermission(
    const GURL& source_origin,
    int callback_context,
    int render_process_id,
    int render_view_id) {
}

WebKit::WebNotificationPresenter::Permission
    ContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_url,
        const content::ResourceContext& context) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

void ContentBrowserClient::ShowDesktopNotification(
    const DesktopNotificationHostMsg_Show_Params& params,
    int render_process_id,
    int render_view_id,
    bool worker) {
}

void ContentBrowserClient::CancelDesktopNotification(
    int render_process_id,
    int render_view_id,
    int notification_id) {
}

bool ContentBrowserClient::CanCreateWindow(
    const GURL& source_url,
    WindowContainerType container_type,
    const content::ResourceContext& context) {
  return false;
}

std::string ContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, const content::ResourceContext& context) {
  return std::string();
}

ResourceDispatcherHost* ContentBrowserClient::GetResourceDispatcherHost() {
  static ResourceQueue::DelegateSet temp;
  static ResourceDispatcherHost rdh(temp);
  return &rdh;
}

ui::Clipboard* ContentBrowserClient::GetClipboard() {
  static ui::Clipboard clipboard;
  return &clipboard;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
int ContentBrowserClient::GetCrashSignalFD(const std::string& process_type) {
  return -1;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

}  // namespace content
