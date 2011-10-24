// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A collection of classes that are useful when testing things that use a
// GaiaAuthFetcher.

#ifndef CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_UNITTEST_H_
#define CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_UNITTEST_H_
#pragma once

#include <string>

#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/http_return.h"
#include "content/common/net/url_fetcher.h"
#include "content/test/test_url_fetcher_factory.h"
#include "net/url_request/url_request_status.h"

namespace content {
class URLFetcherDelegate;
}

// Responds as though ClientLogin returned from the server.
class MockFetcher : public URLFetcher {
 public:
  MockFetcher(bool success,
              const GURL& url,
              const std::string& results,
              URLFetcher::RequestType request_type,
              content::URLFetcherDelegate* d);

  MockFetcher(const GURL& url,
              const net::URLRequestStatus& status,
              int response_code,
              const net::ResponseCookies& cookies,
              const std::string& results,
              URLFetcher::RequestType request_type,
              content::URLFetcherDelegate* d);

  virtual ~MockFetcher();

  virtual void Start();

  virtual const GURL& url() const;
  virtual const net::URLRequestStatus& status() const;
  virtual int response_code() const;
  virtual const net::ResponseCookies& cookies() const;
  virtual bool GetResponseAsString(std::string* out_response_string) const;

 private:
  GURL url_;
  net::URLRequestStatus status_;
  int response_code_;
  net::ResponseCookies cookies_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(MockFetcher);
};

template<typename T>
class MockFactory : public URLFetcher::Factory,
                    public ScopedURLFetcherFactory {
 public:
  MockFactory()
      : ScopedURLFetcherFactory(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
        success_(true) {
  }
  ~MockFactory() {}
  URLFetcher* CreateURLFetcher(int id,
                               const GURL& url,
                               URLFetcher::RequestType request_type,
                               content::URLFetcherDelegate* d) {
    return new T(success_, url, results_, request_type, d);
  }
  void set_success(bool success) {
    success_ = success;
  }
  void set_results(const std::string& results) {
    results_ = results;
  }
 private:
  bool success_;
  std::string results_;
  DISALLOW_COPY_AND_ASSIGN(MockFactory);
};

#endif  // CHROME_COMMON_NET_GAIA_GAIA_AUTH_FETCHER_UNITTEST_H_
