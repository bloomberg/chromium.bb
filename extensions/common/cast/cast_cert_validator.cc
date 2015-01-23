// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/cast/cast_cert_validator.h"

namespace extensions {
namespace core_api {
namespace cast_crypto {

VerificationResult::VerificationResult()
    : VerificationResult("", ERROR_NONE, 0) {
}

VerificationResult::VerificationResult(const std::string& in_error_message,
                                       ErrorType in_error_type)
    : VerificationResult(in_error_message, in_error_type, 0) {
}

VerificationResult::VerificationResult(const std::string& in_error_message,
                                       ErrorType in_error_type,
                                       int in_error_code)
    : error_type(in_error_type),
      error_message(in_error_message),
      library_error_code(in_error_code) {
}

}  // namespace cast_crypto
}  // namespace core_api
}  // namespace extensions
