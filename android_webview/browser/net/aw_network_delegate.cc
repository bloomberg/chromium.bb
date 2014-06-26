// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_network_delegate.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "base/android/build_info.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_auth_request_handler.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/browser/data_reduction_proxy_protocol.h"
#include "net/base/net_errors.h"
#include "net/base/completion_callback.h"
#include "net/proxy/proxy_info.h"
#include "net/proxy/proxy_server.h"
#include "net/url_request/url_request.h"

namespace android_webview {

AwNetworkDelegate::AwNetworkDelegate()
    : data_reduction_proxy_params_(NULL),
      data_reduction_proxy_auth_request_handler_(NULL) {
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

void AwNetworkDelegate::OnBeforeSendProxyHeaders(
    net::URLRequest* request,
    const net::ProxyInfo& proxy_info,
    net::HttpRequestHeaders* headers) {
  if (data_reduction_proxy_auth_request_handler_) {
    data_reduction_proxy_auth_request_handler_->MaybeAddRequestHeader(
        request, proxy_info.proxy_server(), headers);
  }
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

  data_reduction_proxy::MaybeBypassProxyAndPrepareToRetry(
      data_reduction_proxy_params_,
      request,
      original_response_headers,
      override_response_headers);

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

int AwNetworkDelegate::OnBeforeSocketStreamConnect(
    net::SocketStream* stream,
    const net::CompletionCallback& callback) {
  return net::OK;
}

}  // namespace android_webview
