// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_WEBDRIVER_DISPATCH_H_
#define CHROME_TEST_WEBDRIVER_WEBDRIVER_DISPATCH_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chrome/test/webdriver/commands/response.h"
#include "third_party/mongoose/mongoose.h"

namespace base {
class DictionaryValue;
class WaitableEvent;
}

namespace webdriver {

class Command;
class HttpResponse;

namespace mongoose {

typedef void (HttpCallback)(struct mg_connection* connection,
                            const struct mg_request_info* request_info,
                            void* user_data);

struct CallbackDetails {
  CallbackDetails() {
  }

  CallbackDetails(const std::string &uri_regex,
                  HttpCallback* func,
                  void* user_data)
    : uri_regex_(uri_regex),
      func_(func),
      user_data_(user_data) {
  }

  std::string uri_regex_;
  HttpCallback* func_;
  void* user_data_;
};

}  // namespace mongoose

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
                      struct mg_connection* const connection,
                      std::string* method,
                      std::vector<std::string>* path_segments,
                      base::DictionaryValue** parameters,
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
  base::DictionaryValue* parameters = NULL;
  Response response;
  if (internal::ParseRequestInfo(request_info,
                                 connection,
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
  explicit Dispatcher(const std::string& root);
  ~Dispatcher();

  bool ProcessHttpRequest(struct mg_connection* conn,
                          const struct mg_request_info* request_info);

  // Registers a callback for a WebDriver command using the given URL |pattern|.
  // The |CommandType| must be a subtype of |webdriver::Command|.
  template<typename CommandType>
  void Add(const std::string& pattern);

  // Registers a callback that will shutdown the server.  When any HTTP request
  // is received at this URL |pattern|, the |shutdown_event| will be signaled.
  void AddShutdown(const std::string& pattern,
                   base::WaitableEvent* shutdown_event);

  // Registers a callback that responds to with this server's status
  // information, as defined by the WebDriver wire protocol:
  // http://code.google.com/p/selenium/wiki/JsonWireProtocol#GET_/status.
  void AddStatus(const std::string& pattern);

  // Registers a callback for the given pattern that will return the current
  // WebDriver log contents.
  void AddLog(const std::string& pattern);

  // Registers a callback that will always respond with a
  // "HTTP/1.1 501 Not Implemented" message.
  void SetNotImplemented(const std::string& pattern);

  // Registers a callback that will respond for all other requests with a
  // "HTTP/1.1 403 Forbidden" message. Should be called only after registering
  // other callbacks.
  void ForbidAllOtherRequests();

 private:
  void AddCallback(const std::string& uri_pattern,
                   webdriver::mongoose::HttpCallback callback,
                   void* user_data);

  std::vector<webdriver::mongoose::CallbackDetails> callbacks_;
  const std::string url_base_;

  DISALLOW_COPY_AND_ASSIGN(Dispatcher);
};


template <typename CommandType>
void Dispatcher::Add(const std::string& pattern) {
  AddCallback(url_base_ + pattern, &Dispatch<CommandType>, NULL);
}

}  // namespace webdriver

#endif  // CHROME_TEST_WEBDRIVER_WEBDRIVER_DISPATCH_H_
