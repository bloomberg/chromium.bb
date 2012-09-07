// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/net/aw_network_delegate.h"

#include "android_webview/browser/aw_cookie_access_policy.h"
#include "net/base/net_errors.h"
#include "net/base/completion_callback.h"
#include "net/url_request/url_request.h"

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
  return net::OK;
}

void AwNetworkDelegate::OnSendHeaders(net::URLRequest* request,
                                      const net::HttpRequestHeaders& headers) {
}

int AwNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers) {
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
                                         const string16& error) {
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
                                        const FilePath& path) const {
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

void AwNetworkDelegate::OnRequestWaitStateChange(const net::URLRequest& request,
                                                 RequestWaitState state) {
}

}  // namespace android_webview
