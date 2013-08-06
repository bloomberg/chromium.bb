// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_

#include <string>
#include <vector>

#include "base/callback.h"

namespace base {

class DictionaryValue;

}  // namespace base

namespace cloud_print_response_parser {

struct Job {
  Job();
  ~Job();

  std::string job_id;
  std::string create_time;
  std::string file_url;
  std::string ticket_url;
  std::string title;

  // Downloaded data:
  std::string file;
  std::string ticket;
};

// Parses CloudPrint register start response to out parameters.
// Returns |true| on success. Callback is called with description as a parameter
// when parsing is failed.
bool ParseRegisterStartResponse(const std::string& response,
                                std::string* error_description,
                                std::string* polling_url,
                                std::string* registration_token,
                                std::string* complete_invite_url,
                                std::string* device_id);

// Parses CloudPrint register complete response to out parameters.
// Returns |true| on success. Callback is called with description as a parameter
// when parsing is failed.
bool ParseRegisterCompleteResponse(const std::string& response,
                                   std::string* error_description,
                                   std::string* authorization_code,
                                   std::string* xmpp_jid);

// Parses CloudPrint fetch response to out parameters.
// Returns |true| on success. Callback is called with description as a parameter
// when parsing is failed.
bool ParseFetchResponse(const std::string& response,
                        std::string* error_description,
                        std::vector<Job>* list);

}  // namespace cloud_print_response_parser

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_CLOUD_PRINT_RESPONSE_PARSER_H_

