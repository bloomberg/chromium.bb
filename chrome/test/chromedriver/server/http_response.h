// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_RESPONSE_H_
#define CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_RESPONSE_H_

#include <map>
#include <string>

#include "base/basictypes.h"

class HttpResponse {
 public:
  typedef std::map<std::string, std::string> HeaderMap;

  // The supported HTTP response codes.
  static const int kOk;
  static const int kNoContent;
  static const int kSeeOther;
  static const int kNotModified;
  static const int kBadRequest;
  static const int kForbidden;
  static const int kNotFound;
  static const int kMethodNotAllowed;
  static const int kInternalServerError;
  static const int kNotImplemented;

  // Creates an HTTP response with a 200 OK status.
  HttpResponse();
  explicit HttpResponse(int status);
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

  // Returns the status phrase recommended by RFC 2616 section 6.1.1 for this
  // response's status code. If the status code is not recognized, the default
  // "Unknown" status phrase will be used.
  std::string GetReasonPhrase() const;

  // Appends this response to |data|, abiding by HTTP 1.1.
  // This will add an appropriate "Content-Length" header if not already set.
  void GetData(std::string* data) const;

  int status() const;
  void set_status(int status);
  const std::string& body() const;
  void set_body(const std::string& body);

 private:
  void UpdateHeader(const std::string& name, const std::string& new_value);

  int status_;
  HeaderMap headers_;
  std::string body_;
};

#endif  // CHROME_TEST_CHROMEDRIVER_SERVER_HTTP_RESPONSE_H_
