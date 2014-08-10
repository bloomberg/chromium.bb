// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/cast_channel/cast_auth_util.h"

namespace extensions {
namespace core_api {
namespace cast_channel {

AuthResult::AuthResult() : error_type(ERROR_NONE), nss_error_code(0) {
}

AuthResult::~AuthResult() {
}

// static
AuthResult AuthResult::Create(const std::string& error_message,
                              ErrorType error_type) {
  return AuthResult(error_message, error_type, 0);
}

// static
AuthResult AuthResult::CreateWithNSSError(const std::string& error_message,
                                          ErrorType error_type,
                                          int nss_error_code) {
  return AuthResult(error_message, error_type, nss_error_code);
}

AuthResult::AuthResult(const std::string& error_message,
                       ErrorType error_type,
                       int nss_error_code)
    : error_message(error_message),
      error_type(error_type),
      nss_error_code(nss_error_code) {
}

}  // namespace cast_channel
}  // namespace core_api
}  // namespace extensions
