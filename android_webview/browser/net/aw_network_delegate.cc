// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_network_delegate.h"

#include "android_webview/browser/aw_contents_io_thread_client.h"
#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/android/build_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/resource_request_info.h"
#include "net/base/net_errors.h"
#include "net/base/completion_callback.h"
#include "net/http/http_response_headers.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"

using content::BrowserThread;

namespace android_webview {

AwNetworkDelegate::AwNetworkDelegate() {
}

AwNetworkDelegate::~AwNetworkDelegate() {
}

int AwNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return net::OK;
}

int AwNetworkDelegate::OnBeforeSendHeaders(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {

  DCHECK(headers);
  headers->SetHeaderIfMissing(
      "X-Requested-With",
      base::android::BuildInfo::GetInstance()->package_name());
  return net::OK;
}

void AwNetworkDelegate::OnSendHeaders(net::URLRequest* request,
                                      const net::HttpRequestHeaders& headers) {
}

int AwNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  int render_process_id, render_frame_id;
  if (original_response_headers->response_code() >= 400 &&
      content::ResourceRequestInfo::GetRenderFrameForRequest(
          request, &render_process_id, &render_frame_id)) {
    scoped_ptr<AwContentsIoThreadClient> io_thread_client =
        AwContentsIoThreadClient::FromID(render_process_id, render_frame_id);
    if (io_thread_client.get()) {
      io_thread_client->OnReceivedHttpError(request, original_response_headers);
    }
  }
  return net::OK;
}

void AwNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                         const GURL& new_location) {
}

void AwNetworkDelegate::OnResponseStarted(net::URLRequest* request) {
}

void AwNetworkDelegate::OnRawBytesRead(const net::URLRequest& request,
                                       int bytes_read) {
}

void AwNetworkDelegate::OnCompleted(net::URLRequest* request, bool started) {
}

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

bool AwNetworkDelegate::OnCanThrottleRequest(
    const net::URLRequest& request) const {
  return false;
}

}  // namespace android_webview
