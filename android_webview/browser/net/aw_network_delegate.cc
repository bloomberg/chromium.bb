// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_network_delegate.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_contents_client_bridge_base.h"
#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "android_webview/browser/net/aw_web_resource_request.h"
#include "base/android/build_info.h"
#include "components/policy/core/browser/url_blacklist_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace android_webview {

namespace {

void OnReceivedHttpErrorOnUiThread(
    const content::ResourceRequestInfo::WebContentsGetter& web_contents_getter,
    const AwWebResourceRequest& request,
    scoped_refptr<const net::HttpResponseHeaders> original_response_headers) {
  AwContentsClientBridgeBase* client =
      AwContentsClientBridgeBase::FromWebContentsGetter(web_contents_getter);
  if (!client) {
    DLOG(WARNING) << "client is null, onReceivedHttpError dropped for "
                  << request.url;
    return;
  }
  client->OnReceivedHttpError(request, original_response_headers);
}

}  // namespace

AwNetworkDelegate::AwNetworkDelegate() : url_blacklist_manager_(nullptr) {
}

AwNetworkDelegate::~AwNetworkDelegate() {
}

int AwNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  if (!url_blacklist_manager_) {
    url_blacklist_manager_ =
        AwBrowserContext::GetDefault()->GetURLBlacklistManager();
  }
  if (url_blacklist_manager_->IsURLBlocked(request->url()))
    return net::ERR_BLOCKED_BY_ADMINISTRATOR;

  return net::OK;
}

int AwNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  DCHECK(headers);
  headers->SetHeaderIfMissing(
      "X-Requested-With",
      base::android::BuildInfo::GetInstance()->package_name());
  return net::OK;
}

void AwNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {}

int AwNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (original_response_headers->response_code() >= 400) {
    const content::ResourceRequestInfo* request_info =
        content::ResourceRequestInfo::ForRequest(request);
    // A request info may not exist for requests not originating from content.
    if (request_info == nullptr)
        return net::OK;
    // keep a ref before binding and posting to UI thread.
    scoped_refptr<const net::HttpResponseHeaders> response_headers(
        original_response_headers);
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OnReceivedHttpErrorOnUiThread,
                   request_info->GetWebContentsGetterForRequest(),
                   AwWebResourceRequest(*request), response_headers));
  }
  return net::OK;
}

void AwNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                         const GURL& new_location) {
}

void AwNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                          int net_error) {}

void AwNetworkDelegate::OnCompleted(net::URLRequest* request,
                                    bool started,
                                    int net_error) {}

void AwNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {
}

void AwNetworkDelegate::OnPACScriptError(int line_number,
                                         const base::string16& error) {
}

net::NetworkDelegate::AuthRequiredResponse AwNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool AwNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                        const net::CookieList& cookie_list) {
  return AwCookieAccessPolicy::GetInstance()->OnCanGetCookies(request,
                                                              cookie_list);
}

bool AwNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                       const std::string& cookie_line,
                                       net::CookieOptions* options) {
  return AwCookieAccessPolicy::GetInstance()->OnCanSetCookie(request,
                                                             cookie_line,
                                                             options);
}

bool AwNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                        const base::FilePath& path) const {
  return true;
}

}  // namespace android_webview
