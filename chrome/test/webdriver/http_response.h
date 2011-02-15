// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_WEBDRIVER_HTTP_RESPONSE_H_
#define CHROME_TEST_WEBDRIVER_HTTP_RESPONSE_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"

namespace webdriver {

class HttpResponse {
 public:
  typedef std::map<std::string, std::string> HeaderMap;

  // The supported HTTP response codes.
  static const int kOk;
  static const int kNoContent;
  static const int kSeeOther;
  static const int kNotModified;
  static const int kBadRequest;
  static const int kNotFound;
  static const int kMethodNotAllowed;
  static const int kInternalServerError;

  HttpResponse();
  ~HttpResponse();

  // Sets a header in this response. If a header with the same |name| already
  // exists, the |value| will be appended to the existing values. Since header
  // names are case insensitive, the header will be stored in lowercase format.
  void AddHeader(const std::string& name, const std::string& value);

  // Retrieves the value of the specified header. If there is no such header,
  // the output |value| will not be modified and false will be returned.
  bool GetHeader(const std::string& name, std::string* value) const;

  // Removes the header with the given |name|. Returns whether there was a
  // matching header to remove.
  bool RemoveHeader(const std::string& name);

  // Removes all headers.
  void ClearHeaders();

  // Convenience function for setting the Content-Type header for this response.
  void SetMimeType(const std::string& mime_type);

  // Sets the message body for this response; will also set the "Content-Length"
  // message header.
  void SetBody(const std::string& data);
  void SetBody(const char* const data, size_t length);

  // Returns the status phrase recommended by RFC 2616 section 6.1.1 for this
  // response's status code. If the status code is not recognized, the default
  // "Unknown" status phrase will be used.
  std::string GetReasonPhrase() const;

  int status() const;
  void set_status(int status);
  const HeaderMap* const headers() const;
  const char* const data() const;
  size_t length() const;

 private:
  void UpdateHeader(const std::string& name, const std::string& new_value);

  int status_;
  HeaderMap headers_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(HttpResponse);
};

}  // webdriver

#endif  // CHROME_TEST_WEBDRIVER_HTTP_RESPONSE_H_
