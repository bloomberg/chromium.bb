// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_auth_handler_negotiate.h"

#include "base/logging.h"

namespace net {

// TODO(cbentzel): Negotiate authentication protocol is not supported on Posix
// systems currently. These stubs make the main HTTP Authentication code bypass
// Negotiate without requiring conditional compilation.

HttpAuthHandlerNegotiate::HttpAuthHandlerNegotiate() {
}

HttpAuthHandlerNegotiate::~HttpAuthHandlerNegotiate() {
}

bool HttpAuthHandlerNegotiate::NeedsIdentity() {
  NOTREACHED();
  return false;
}

bool HttpAuthHandlerNegotiate::IsFinalRound() {
  NOTREACHED();
  return false;
}

bool HttpAuthHandlerNegotiate::Init(std::string::const_iterator challenge_begin,
                                    std::string::const_iterator challenge_end) {
  return false;
}

std::string HttpAuthHandlerNegotiate::GenerateCredentials(
    const std::wstring& username,
    const std::wstring& password,
    const HttpRequestInfo* request,
    const ProxyInfo* proxy) {
  NOTREACHED();
  return std::string();
}

}  // namespace net
