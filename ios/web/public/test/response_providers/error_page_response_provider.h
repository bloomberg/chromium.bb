// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_ERROR_PAGE_RESPONSE_PROVIDER_H_
#define IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_ERROR_PAGE_RESPONSE_PROVIDER_H_

#include <map>
#include <string>

#import "ios/web/public/test/response_providers/html_response_provider.h"
#include "url/gurl.h"

// A HtmlResponseProvider that supports the following additional URLs:
// - GetRedirectToDnsFailureUrl - the response is a redirect to
//   |GetDnsFailureUrl|.
// - GetDnsFailureUrl - triggers a DNS error.
class ErrorPageResponseProvider : public HtmlResponseProvider {
 public:
  ErrorPageResponseProvider() : HtmlResponseProvider() {}
  explicit ErrorPageResponseProvider(
      const std::map<GURL, std::string>& responses)
      : HtmlResponseProvider(responses) {}
  // Returns a URL that causes a DNS failure.
  static GURL GetDnsFailureUrl();
  // Returns a URL that redirects to a bad URL.
  static GURL GetRedirectToDnsFailureUrl();

  // HtmlResponseProvider implementation.
  bool CanHandleRequest(const Request& request) override;
  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override;
};

#endif  // IOS_WEB_PUBLIC_TEST_RESPONSE_PROVIDERS_ERROR_PAGE_RESPONSE_PROVIDER_H_
