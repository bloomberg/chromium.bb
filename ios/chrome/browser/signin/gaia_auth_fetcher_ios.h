// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_
#define IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_

#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"

class GaiaAuthFetcherIOSBridge;

namespace web {
class BrowserState;
}

// Specialization of GaiaAuthFetcher on iOS.
//
// Authenticate a user against the Google Accounts ClientLogin API
// with various capabilities and return results to a GaiaAuthConsumer.
//
// If necessary (cookies needed and WKWebView enabled), the queries will
// be fetched via a WKWebView instead of a net::URLFetcher.
class GaiaAuthFetcherIOS : public GaiaAuthFetcher {
 public:
  GaiaAuthFetcherIOS(GaiaAuthConsumer* consumer,
                     const std::string& source,
                     net::URLRequestContextGetter* getter,
                     web::BrowserState* browser_state);
  ~GaiaAuthFetcherIOS() override;

  void CancelRequest() override;

 private:
  friend class GaiaAuthFetcherIOSBridge;
  friend class GaiaAuthFetcherIOSTest;

  void CreateAndStartGaiaFetcher(const std::string& body,
                                 const std::string& headers,
                                 const GURL& gaia_gurl,
                                 int load_flags) override;
  void FetchComplete(const GURL& url,
                     const std::string& data,
                     const net::ResponseCookies& cookies,
                     const net::URLRequestStatus& status,
                     int response_code);

  scoped_ptr<GaiaAuthFetcherIOSBridge> bridge_;
  web::BrowserState* browser_state_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAuthFetcherIOS);
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_GAIA_AUTH_FETCHER_IOS_H_
