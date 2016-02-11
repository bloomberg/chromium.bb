// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_RESPONSE_PROVIDER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"

@class GCDWebServerResponse;

namespace web {

// An abstract class for a provider that services a request and returns a
// GCDWebServerResponse.
// Note: The ResponseProviders can be called from any arbitrary GCD thread.
class ResponseProvider {
 public:
  // A data structure that encapsulated all the fields of a request.
  struct Request {
    Request(const GURL& url,
            const std::string& method,
            const std::string& body,
            const net::HttpRequestHeaders& headers);
    virtual ~Request();

    // The URL for the request.
    GURL url;
    // The HTTP method for the request such as "GET" or "POST".
    std::string method;
    // The body of the request.
    std::string body;
    // The HTTP headers for the request.
    net::HttpRequestHeaders headers;
  };

  // Returns true if the request is handled by the provider.
  virtual bool CanHandleRequest(const Request& request) = 0;

  // Returns the GCDWebServerResponse as a reply to the request. Will only be
  // called if the provider can handle the request.
  virtual GCDWebServerResponse* GetGCDWebServerResponse(
      const Request& request) = 0;

  // Gets default response headers with a text/html content type and a 200
  // response code.
  static scoped_refptr<net::HttpResponseHeaders> GetDefaultResponseHeaders();
  // Gets configurable response headers with a provided content type and a
  // 200 response code.
  static scoped_refptr<net::HttpResponseHeaders> GetResponseHeaders(
      const std::string& content_type);
  // Gets configurable response headers with a provided content type and
  // response code.
  static scoped_refptr<net::HttpResponseHeaders> GetResponseHeaders(
      const std::string& content_type,
      net::HttpStatusCode response_code);
  // Gets configurable response based on |http_status| headers for redirecting
  // to |destination|.
  static scoped_refptr<net::HttpResponseHeaders> GetRedirectResponseHeaders(
      const std::string& destination,
      const net::HttpStatusCode& http_status);

  ResponseProvider();
  virtual ~ResponseProvider() {};
 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseProvider);
};

}  // namspace web

#endif  // IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_RESPONSE_PROVIDER_H_
