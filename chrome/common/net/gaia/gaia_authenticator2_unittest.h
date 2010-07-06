// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A collection of classes that are useful when testing things that use a
// GaiaAuthenticator2.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_UNITTEST_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_UNITTEST_H_

#include "chrome/common/net/gaia/gaia_authenticator2.h"
#include "chrome/common/net/url_fetcher.h"
#include "chrome/common/net/http_return.h"
#include "net/url_request/url_request_status.h"

// Responds as though ClientLogin returned from the server.
class MockFetcher : public URLFetcher {
 public:
  MockFetcher(bool success,
              const GURL& url,
              URLFetcher::RequestType request_type,
              URLFetcher::Delegate* d)
      : URLFetcher(url, request_type, d),
        success_(success),
        url_(url) {}
  ~MockFetcher() {}
  void Start() {
    URLRequestStatus::Status code;
    int http_code;
    if (success_) {
      http_code = RC_REQUEST_OK;
      code = URLRequestStatus::SUCCESS;
    } else {
      http_code = RC_FORBIDDEN;
      code = URLRequestStatus::FAILED;
    }

    URLRequestStatus status(code, 0);
    delegate()->OnURLFetchComplete(NULL,
                                   url_,
                                   status,
                                   http_code,
                                   ResponseCookies(),
                                   std::string());
  }
 private:
  bool success_;
  GURL url_;
  DISALLOW_COPY_AND_ASSIGN(MockFetcher);
};

class MockFactory : public URLFetcher::Factory {
 public:
  MockFactory()
      : success_(true) {}
  ~MockFactory() {}
  URLFetcher* CreateURLFetcher(int id,
                               const GURL& url,
                               URLFetcher::RequestType request_type,
                               URLFetcher::Delegate* d) {
    return new MockFetcher(success_, url, request_type, d);
  }
  void set_success(bool success) {
    success_ = success;
  }
 private:
  bool success_;
  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTHENTICATOR2_UNITTEST_H_
