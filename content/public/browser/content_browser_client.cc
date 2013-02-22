// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include "base/file_path.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

BrowserMainParts* ContentBrowserClient::CreateBrowserMainParts(
    const MainFunctionParams& parameters) {
  return NULL;
}

WebContentsView* ContentBrowserClient::OverrideCreateWebContentsView(
    WebContents* web_contents,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  return NULL;
}

WebContentsViewDelegate* ContentBrowserClient::GetWebContentsViewDelegate(
    WebContents* web_contents) {
  return NULL;
}

GURL ContentBrowserClient::GetEffectiveURL(BrowserContext* browser_context,
                                           const GURL& url) {
  return url;
}

bool ContentBrowserClient::ShouldUseProcessPerSite(
    BrowserContext* browser_context, const GURL& effective_url) {
  return false;
}

net::URLRequestContextGetter* ContentBrowserClient::CreateRequestContext(
    BrowserContext* browser_context,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        blob_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        file_system_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        developer_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_protocol_handler,
    scoped_ptr<net::URLRequestJobFactory::ProtocolHandler>
        chrome_devtools_protocol_handler) {
  return NULL;
}

net::URLRequestContextGetter*
ContentBrowserClient::CreateRequestContextForStoragePartition(
    BrowserContext* browser_context,
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
        chrome_devtools_protocol_handler) {
  return NULL;
}

bool ContentBrowserClient::IsHandledURL(const GURL& url) {
  return false;
}

bool ContentBrowserClient::IsSuitableHost(RenderProcessHost* process_host,
                                          const GURL& site_url) {
  return true;
}

bool ContentBrowserClient::ShouldTryToUseExistingProcessHost(
      BrowserContext* browser_context, const GURL& url) {
  return false;
}

bool ContentBrowserClient::ShouldSwapProcessesForNavigation(
    SiteInstance* site_instance,
    const GURL& current_url,
    const GURL& new_url) {
  return false;
}

bool ContentBrowserClient::ShouldSwapProcessesForRedirect(
    ResourceContext* resource_context, const GURL& current_url,
    const GURL& new_url) {
  return false;
}

std::string ContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return std::string();
}

std::string ContentBrowserClient::GetApplicationLocale() {
  return "en-US";
}

std::string ContentBrowserClient::GetAcceptLangs(BrowserContext* context) {
  return std::string();
}

gfx::ImageSkia* ContentBrowserClient::GetDefaultFavicon() {
  static gfx::ImageSkia* empty = new gfx::ImageSkia();
  return empty;
}

bool ContentBrowserClient::AllowAppCache(const GURL& manifest_url,
                                         const GURL& first_party,
                                         ResourceContext* context) {
  return true;
}

bool ContentBrowserClient::AllowGetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const net::CookieList& cookie_list,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_view_id) {
  return true;
}

bool ContentBrowserClient::AllowSetCookie(const GURL& url,
                                          const GURL& first_party,
                                          const std::string& cookie_line,
                                          ResourceContext* context,
                                          int render_process_id,
                                          int render_view_id,
                                          net::CookieOptions* options) {
  return true;
}

bool ContentBrowserClient::AllowSaveLocalState(ResourceContext* context) {
  return true;
}

bool ContentBrowserClient::AllowWorkerDatabase(
    const GURL& url,
    const string16& name,
    const string16& display_name,
    unsigned long estimated_size,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ContentBrowserClient::AllowWorkerFileSystem(
    const GURL& url,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

bool ContentBrowserClient::AllowWorkerIndexedDB(
    const GURL& url,
    const string16& name,
    ResourceContext* context,
    const std::vector<std::pair<int, int> >& render_views) {
  return true;
}

QuotaPermissionContext* ContentBrowserClient::CreateQuotaPermissionContext() {
  return NULL;
}

net::URLRequestContext* ContentBrowserClient::OverrideRequestContextForURL(
    const GURL& url, ResourceContext* context) {
  return NULL;
}

std::string ContentBrowserClient::GetStoragePartitionIdForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  return std::string();
}

bool ContentBrowserClient::IsValidStoragePartitionId(
    BrowserContext* browser_context,
    const std::string& partition_id) {
  // Since the GetStoragePartitionIdForChildProcess() only generates empty
  // strings, we should only ever see empty strings coming back.
  return partition_id.empty();
}

void ContentBrowserClient::GetStoragePartitionConfigForSite(
    BrowserContext* browser_context,
    const GURL& site,
    bool can_be_default,
    std::string* partition_domain,
    std::string* partition_name,
    bool* in_memory) {
  partition_domain->clear();
  partition_name->clear();
  *in_memory = false;
}

MediaObserver* ContentBrowserClient::GetMediaObserver() {
  return NULL;
}

WebKit::WebNotificationPresenter::Permission
    ContentBrowserClient::CheckDesktopNotificationPermission(
        const GURL& source_origin,
        ResourceContext* context,
        int render_process_id) {
  return WebKit::WebNotificationPresenter::PermissionAllowed;
}

bool ContentBrowserClient::CanCreateWindow(const GURL& opener_url,
                                           const GURL& origin,
                                           WindowContainerType container_type,
                                           ResourceContext* context,
                                           int render_process_id,
                                           bool* no_javascript_access) {
  *no_javascript_access = false;
  return true;
}

std::string ContentBrowserClient::GetWorkerProcessTitle(
    const GURL& url, ResourceContext* context) {
  return std::string();
}

SpeechRecognitionManagerDelegate*
    ContentBrowserClient::GetSpeechRecognitionManagerDelegate() {
  return NULL;
}

net::NetLog* ContentBrowserClient::GetNetLog() {
  return NULL;
}

AccessTokenStore* ContentBrowserClient::CreateAccessTokenStore() {
  return NULL;
}

bool ContentBrowserClient::IsFastShutdownPossible() {
  return true;
}

base::FilePath ContentBrowserClient::GetDefaultDownloadDirectory() {
  return base::FilePath();
}

std::string ContentBrowserClient::GetDefaultDownloadName() {
  return std::string();
}

BrowserPpapiHost*
    ContentBrowserClient::GetExternalBrowserPpapiHost(int plugin_process_id) {
  return NULL;
}

bool ContentBrowserClient::AllowPepperSocketAPI(
    BrowserContext* browser_context,
    const GURL& url,
    const SocketPermissionRequest& params) {
  return false;
}

base::FilePath ContentBrowserClient::GetHyphenDictionaryDirectory() {
  return base::FilePath();
}

ui::SelectFilePolicy* ContentBrowserClient::CreateSelectFilePolicy(
    WebContents* web_contents) {
  return NULL;
}

#if defined(OS_WIN)
const wchar_t* ContentBrowserClient::GetResourceDllName() {
  return NULL;
}
#endif

#if defined(USE_NSS)
crypto::CryptoModuleBlockingPasswordDelegate*
    ContentBrowserClient::GetCryptoPasswordDelegate(const GURL& url) {
  return NULL;
}
#endif

}  // namespace content
