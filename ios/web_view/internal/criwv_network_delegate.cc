// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web_view/internal/criwv_network_delegate.h"

#include "net/base/net_errors.h"

namespace ios_web_view {

CRIWVNetworkDelegate::CRIWVNetworkDelegate() {}

CRIWVNetworkDelegate::~CRIWVNetworkDelegate() {}

int CRIWVNetworkDelegate::OnBeforeURLRequest(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    GURL* new_url) {
  return net::OK;
}

int CRIWVNetworkDelegate::OnBeforeStartTransaction(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    net::HttpRequestHeaders* headers) {
  return net::OK;
}

void CRIWVNetworkDelegate::OnStartTransaction(
    net::URLRequest* request,
    const net::HttpRequestHeaders& headers) {}

int CRIWVNetworkDelegate::OnHeadersReceived(
    net::URLRequest* request,
    const net::CompletionCallback& callback,
    const net::HttpResponseHeaders* original_response_headers,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    GURL* allowed_unsafe_redirect_url) {
  return net::OK;
}

void CRIWVNetworkDelegate::OnBeforeRedirect(net::URLRequest* request,
                                            const GURL& new_location) {}

void CRIWVNetworkDelegate::OnResponseStarted(net::URLRequest* request) {}

void CRIWVNetworkDelegate::OnNetworkBytesReceived(net::URLRequest* request,
                                                  int64_t bytes_received) {}

void CRIWVNetworkDelegate::OnCompleted(net::URLRequest* request, bool started) {
}

void CRIWVNetworkDelegate::OnURLRequestDestroyed(net::URLRequest* request) {}

void CRIWVNetworkDelegate::OnPACScriptError(int line_number,
                                            const base::string16& error) {}

CRIWVNetworkDelegate::AuthRequiredResponse CRIWVNetworkDelegate::OnAuthRequired(
    net::URLRequest* request,
    const net::AuthChallengeInfo& auth_info,
    const AuthCallback& callback,
    net::AuthCredentials* credentials) {
  return AUTH_REQUIRED_RESPONSE_NO_ACTION;
}

bool CRIWVNetworkDelegate::OnCanGetCookies(const net::URLRequest& request,
                                           const net::CookieList& cookie_list) {
  return true;
}

bool CRIWVNetworkDelegate::OnCanSetCookie(const net::URLRequest& request,
                                          const std::string& cookie_line,
                                          net::CookieOptions* options) {
  return true;
}

bool CRIWVNetworkDelegate::OnCanAccessFile(const net::URLRequest& request,
                                           const base::FilePath& path) const {
  return true;
}

}  // namespace ios_web_view
