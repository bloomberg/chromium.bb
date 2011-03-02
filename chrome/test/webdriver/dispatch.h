// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_DISPATCH_H_
#define CHROME_TEST_WEBDRIVER_DISPATCH_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/test/webdriver/commands/response.h"
#include "third_party/mongoose/mongoose.h"

class DictionaryValue;

namespace base {
class WaitableEvent;
}

namespace webdriver {

class Command;
class HttpResponse;

namespace internal {

// Converts a |Response| into a |HttpResponse| to be returned to the client.
// This function is exposed for testing.
void PrepareHttpResponse(const Response& command_response,
                         HttpResponse* const http_response);

// Sends a |response| to a WebDriver command back to the client.
// |connection| is the communication pipe to the HTTP server and
// |request_info| contains any data sent by the user.
void SendResponse(struct mg_connection* const connection,
                  const std::string& request_method,
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

  internal::SendResponse(connection,
                         request_info->request_method,
                         response);
}

class Dispatcher {
 public:
  // Creates a new dispatcher that will register all URL callbacks with the
  // given |context|. Each callback's pattern will be prefixed with the provided
  // |root|.
  Dispatcher(struct mg_context* context, const std::string& root);
  ~Dispatcher();

  // Registers a callback for a WebDriver command using the given URL |pattern|.
  // The |CommandType| must be a subtype of |webdriver::Command|.
  template<typename CommandType>
  void Add(const std::string& pattern);

  // Registers a callback that will shutdown the server.  When any HTTP request
  // is received at this URL |pattern|, the |shutdown_event| will be signaled.
  void AddShutdown(const std::string& pattern,
                   base::WaitableEvent* shutdown_event);

  // Registers a callback that will always respond with a
  // "HTTP/1.1 501 Not Implemented" message.
  void SetNotImplemented(const std::string& pattern);

 private:
  struct mg_context* context_;
  const std::string root_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};


template <typename CommandType>
void Dispatcher::Add(const std::string& pattern) {
  mg_set_uri_callback(context_, (root_ + pattern).c_str(),
                      &Dispatch<CommandType>, NULL);
}

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_DISPATCH_H_
