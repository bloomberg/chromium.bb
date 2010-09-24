// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_DISPATCH_H_
#define CHROME_TEST_WEBDRIVER_DISPATCH_H_

#include <sstream>
#include <string>
#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "chrome/test/webdriver/utility_functions.h"
#include "chrome/test/webdriver/commands/command.h"

#include "third_party/mongoose/mongoose.h"

namespace webdriver {

class Command;

// Sends a |response| to a WebDriver command back to the client.
// |connection| is the communication pipe to the HTTP server and
// |request_info| contains any data sent by the user.
void SendResponse(struct mg_connection* const connection,
                  const struct mg_request_info* const request_info,
                  const Response& response);


// Serves as a link to the mongoose server to find if the user request
// is an HTTP POST, GET, or DELETE and then executes the proper function
// calls for the class that inherts from Command.  An HTTP CREATE is not
// handled and is reserved only for the establishment of a session.
void DispatchCommand(Command* const command, const std::string& method,
                     Response* response);

// Template function for dispatching commands sent to the WebDriver REST
// service. |CommandType| must be a subtype of |webdriver::Command|.
template<typename CommandType>
void Dispatch(struct mg_connection* connection,
              const struct mg_request_info* request_info,
              void* user_data) {
  Response response;

  std::string method(request_info->request_method);

  std::vector<std::string> path_segments;
  std::string uri(request_info->uri);
  SplitString(uri, '/', &path_segments);

  DictionaryValue* parameters = NULL;
  if ((method == "POST" || method == "PUT") &&
      request_info->post_data_len > 0) {
    LOG(INFO) << "...parsing request body";
    std::string json(request_info->post_data, request_info->post_data_len);
    std::string error;
    if (!ParseJSONDictionary(json, &parameters, &error)) {
      response.set_value(Value::CreateStringValue(
          "Failed to parse command data: " + error + "\n  Data: " + json));
      response.set_status(kBadRequest);
      SendResponse(connection, request_info, response);
      return;
    }
  }

  LOG(INFO) << "Dispatching " << method << " " << uri
            << std::string(request_info->post_data,
                           request_info->post_data_len) << std::endl;
  scoped_ptr<CommandType> ptr(new CommandType(path_segments, parameters));
  DispatchCommand(ptr.get(), method, &response);
  SendResponse(connection, request_info, response);
}
}  // namespace webdriver
#endif  // CHROME_TEST_WEBDRIVER_DISPATCH_H_

