// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_
#define CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"
#include "url/gurl.h"

class GURL;
class Profile;

namespace net {
class URLFetcher;
}

class SafeSearchURLReporter : public OAuth2TokenService::Consumer,
                              public net::URLFetcherDelegate {
 public:
  using SuccessCallback = base::Callback<void(bool)>;
  
  SafeSearchURLReporter(OAuth2TokenService* oauth2_token_service,
                        const std::string& account_id,
                        net::URLRequestContextGetter* context);
  ~SafeSearchURLReporter() override;

  static std::unique_ptr<SafeSearchURLReporter> CreateWithProfile(
      Profile* profile);

  void ReportUrl(const GURL& url, const SuccessCallback& callback);

  void set_url_fetcher_id_for_testing(int id) { url_fetcher_id_ = id; }

 private:
  struct Report;
  using ReportIterator = std::vector<std::unique_ptr<Report>>::iterator;

  // OAuth2TokenService::Consumer implementation:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // Requests an access token, which is the first thing we need. This is where
  // we restart when the returned access token has expired.
  void StartFetching(Report* Report);

  void DispatchResult(ReportIterator it, bool success);

  OAuth2TokenService* oauth2_token_service_;
  std::string account_id_;
  net::URLRequestContextGetter* context_;
  int url_fetcher_id_;

  std::vector<std::unique_ptr<Report>> reports_;

  DISALLOW_COPY_AND_ASSIGN(SafeSearchURLReporter);
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_EXPERIMENTAL_SAFE_SEARCH_URL_REPORTER_H_
