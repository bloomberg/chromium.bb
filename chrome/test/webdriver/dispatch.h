// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_DISPATCH_H_
#define CHROME_TEST_WEBDRIVER_DISPATCH_H_

#include <string>
#include <vector>

#include "chrome/test/webdriver/commands/response.h"
#include "third_party/mongoose/mongoose.h"

class DictionaryValue;

namespace webdriver {

class Command;

namespace internal {

// Sends a |response| to a WebDriver command back to the client.
// |connection| is the communication pipe to the HTTP server and
// |request_info| contains any data sent by the user.
void SendResponse(struct mg_connection* const connection,
                  const struct mg_request_info* const request_info,
                  const Response& response);

// Parses the request info and returns whether parsing was successful. If not,
// |response| has been modified with the error.
bool ParseRequestInfo(const struct mg_request_info* const request_info,
                      std::string* method,
                      std::vector<std::string>* path_segments,
                      DictionaryValue** parameters,
                      Response* const response);

// Allows the bulk of the implementation of |Dispatch| to be moved out of this
// header file. Takes ownership of |command|.
void DispatchHelper(Command* const command,
                    const std::string& method,
                    Response* const response);

}  // namespace internal

// Template function for dispatching commands sent to the WebDriver REST
// service. |CommandType| must be a subtype of |webdriver::Command|.
template<typename CommandType>
void Dispatch(struct mg_connection* connection,
              const struct mg_request_info* request_info,
              void* user_data) {
  std::string method;
  std::vector<std::string> path_segments;
  DictionaryValue* parameters = NULL;
  Response response;
  if (internal::ParseRequestInfo(request_info,
                                 &method,
                                 &path_segments,
                                 &parameters,
                                 &response)) {
    internal::DispatchHelper(
        new CommandType(path_segments, parameters),
        method,
        &response);
  }
  internal::SendResponse(connection, request_info, response);
}

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_DISPATCH_H_
