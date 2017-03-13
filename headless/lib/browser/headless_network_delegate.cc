// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_network_delegate.h"

#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"

#include "net/url_request/url_request.h"

namespace headless {

namespace {
// Keep in sync with X_DevTools_Emulate_Network_Conditions_Client_Id defined in
// HTTPNames.json5.
const char kDevToolsEmulateNetworkConditionsClientId[] =
    "X-DevTools-Emulate-Network-Conditions-Client-Id";
}  // namespace

HeadlessNetworkDelegate::HeadlessNetworkDelegate() {}
HeadlessNetworkDelegate::~HeadlessNetworkDelegate() {}

int HeadlessNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  request->RemoveRequestHeaderByName(kDevToolsEmulateNetworkConditionsClientId);
  return net::OK;
}

int HeadlessNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void HeadlessNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {}

int HeadlessNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return net::OK;
}

void HeadlessNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                               const GURL& new_location) {}

void HeadlessNetworkDelegate::OnResponseStarted(net::URLRequest* request,
                                                int net_error) {}

void HeadlessNetworkDelegate::OnCompleted(net::URLRequest* request,
                                          bool started,
                                          int net_error) {}

void HeadlessNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {}

void HeadlessNetworkDelegate::OnPACScriptError(int line_number,
                                               const base::string16& error) {}

net::NetworkDelegate::AuthRequiredResponse
HeadlessNetworkDelegate::OnAuthRequired(net::URLRequest* request,
                                        const net::AuthChallengeInfo& auth_info,
                                        const AuthCallback& callback,
                                        net::AuthCredentials* credentials) {
  return NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool HeadlessNetworkDelegate::OnCanGetCookies(
    const net::URLRequest& request,
    const net::CookieList& cookie_list) {
  return true;
}

bool HeadlessNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                             const std::string& cookie_line,
                                             net::CookieOptions* options) {
  return true;
}

bool HeadlessNetworkDelegate::OnCanAccessFile(
    const net::URLRequest& request,
    const base::FilePath& path) const {
  return true;
}

}  // namespace headless
