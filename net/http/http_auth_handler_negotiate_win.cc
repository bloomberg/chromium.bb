// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "net/http/http_auth_filter.h"

namespace net {

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate(SSPILibrary* library,
                                                   ULONG max_token_length)
    : auth_sspi_(library, "Negotiate", NEGOSSP_NAME, max_token_length) {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

int HttpAuthHandlerNegotiate::GenerateAuthToken(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      &username,
      &password,
      origin_,
      request,
      proxy,
      auth_token);
}

// The Negotiate challenge header looks like:
//   WWW-Authenticate: NEGOTIATE auth-data
bool HttpAuthHandlerNegotiate::Init(HttpAuth::ChallengeTokenizer* challenge) {
  scheme_ = "negotiate";
  score_ = 4;
  properties_ = ENCRYPTS_IDENTITY | IS_CONNECTION_BASED;
  return auth_sspi_.ParseChallenge(challenge);
}

// Require identity on first pass instead of second.
bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  return auth_sspi_.NeedsIdentity();
}

bool HttpAuthHandlerNegotiate::IsFinalRound() {
  return auth_sspi_.IsFinalRound();
}

bool HttpAuthHandlerNegotiate::SupportsDefaultCredentials() {
  return true;
}

int HttpAuthHandlerNegotiate::GenerateDefaultAuthToken(
    const HttpRequestInfo* request,
    const ProxyInfo* proxy,
    std::string* auth_token) {
  return auth_sspi_.GenerateAuthToken(
      NULL,  // username
      NULL,  // password
      origin_,
      request,
      proxy,
      auth_token);
}

HttpAuthHandlerNegotiate::Factory::Factory()
    : max_token_length_(0),
      first_creation_(true),
      is_unsupported_(false),
      sspi_library_(SSPILibrary::GetDefault()) {
}

HttpAuthHandlerNegotiate::Factory::~Factory() {
}

int HttpAuthHandlerNegotiate::Factory::CreateAuthHandler(
    HttpAuth::ChallengeTokenizer* challenge,
    HttpAuth::Target target,
    const GURL& origin,
    scoped_refptr<HttpAuthHandler>* handler) {
  if (is_unsupported_)
    return ERR_UNSUPPORTED_AUTH_SCHEME;
  if (max_token_length_ == 0) {
    int rv = DetermineMaxTokenLength(sspi_library_, NEGOSSP_NAME,
                                     &max_token_length_);
    if (rv == ERR_UNSUPPORTED_AUTH_SCHEME)
      is_unsupported_ = true;
    if (rv != OK)
      return rv;
  }
  // TODO(cbentzel): Move towards model of parsing in the factory
  //                 method and only constructing when valid.
  scoped_refptr<HttpAuthHandler> tmp_handler(
      new HttpAuthHandlerNegotiate(sspi_library_, max_token_length_));
  if (!tmp_handler->InitFromChallenge(challenge, target, origin))
    return ERR_INVALID_RESPONSE;
  handler->swap(tmp_handler);
  return OK;
}

}  // namespace net
