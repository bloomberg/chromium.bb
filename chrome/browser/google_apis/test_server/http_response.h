// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_RESPONSE_H_
#define CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_RESPONSE_H_

#include <map>
#include <string>

#include "base/basictypes.h"

namespace google_apis {
namespace test_server {

enum ResponseCode {
  SUCCESS = 200,
  CREATED = 201,
  NO_CONTENT = 204,
  MOVED = 301,
  RESUME_INCOMPLETE = 308,
  NOT_FOUND = 404,
  PRECONDITION = 412,
  ACCESS_DENIED = 500,
};

// Respresents a HTTP response. Since it can be big, it may be better to use
// scoped_ptr to pass it instead of copying.
class HttpResponse {
 public:
  HttpResponse();
  ~HttpResponse();

  // The response code.
  ResponseCode code() const { return code_; }
  void set_code(ResponseCode code) { code_ = code; }

  // The content of the response.
  const std::string& content() const { return content_; }
  void set_content(const std::string& content) { content_ = content; }

  // The content type.
  const std::string& content_type() const { return content_type_; }
  void set_content_type(const std::string& content_type) {
    content_type_ = content_type;
  }

  // The custom headers to be sent to the client.
  const std::map<std::string, std::string>& custom_headers() const {
    return custom_headers_;
  }

  // Adds a custom header.
  void AddCustomHeader(const std::string& key, const std::string& value) {
    custom_headers_[key] = value;
  }

  // Generates and returns a http response string.
  std::string ToResponseString() const;

 private:
  ResponseCode code_;
  std::string content_;
  std::string content_type_;
  std::map<std::string, std::string> custom_headers_;
};

}  // namespace test_servers
}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_TEST_SERVER_HTTP_RESPONSE_H_
