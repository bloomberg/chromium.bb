// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_

#include <string>

#include "base/callback.h"

namespace base {

class DictionaryValue;

}  // namespace base

namespace cloud_print_response_parser {

// Callback for handling parse errors.
typedef base::Callback<void(const std::string&)> ErrorCallback;

// Parses CloudPrint register start response to out parameters.
// Returns |true| on success. Callback is called with description as a parameter
// when parsing failed.
bool ParseRegisterStartResponse(const std::string& response,
                                const ErrorCallback error_callback,
                                std::string* polling_url_result,
                                std::string* registration_token_result,
                                std::string* complete_invite_url_result,
                                std::string* device_id_result);

// Parses CloudPrint register complete response to out parameters.
// Returns |true| on success. Callback is called with description as a parameter
// when parsing failed.
bool ParseRegisterCompleteResponse(const std::string& response,
                                   const ErrorCallback error_callback,
                                   std::string* authorization_code_result);

}  // namespace cloud_print_response_parser

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_

