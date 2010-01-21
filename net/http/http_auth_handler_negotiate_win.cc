// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "net/base/net_errors.h"

namespace net {

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate() :
    auth_sspi_("Negotiate", NEGOSSP_NAME) {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

std::string HttpAuthHandlerNegotiate::GenerateCredentials(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy) {
  std::string auth_credentials;

  int rv = auth_sspi_.GenerateCredentials(
      username,
      password,
      origin_,
      request,
      proxy,
      &auth_credentials);
  if (rv == OK)
    return auth_credentials;
  return std::string();
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(
    std::string::const_iterator challenge_begin,
    std::string::const_iterator challenge_end) {
  scheme_ = "negotiate";
  score_ = 4;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;
  return auth_sspi_.ParseChallenge(challenge_begin, challenge_end);
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  return auth_sspi_.NeedsIdentity();
}

bool HttpAuthHandlerNegotiate::IsFinalRound() {
  return auth_sspi_.IsFinalRound();
}

}  // namespace net
