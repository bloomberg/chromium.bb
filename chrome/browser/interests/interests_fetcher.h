// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INTERESTS_INTERESTS_FETCHER_H_
#define CHROME_BROWSER_INTERESTS_INTERESTS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_fetcher_delegate.h"

// Class to fetch topics of interest for a user as computed by Google Now. Each
// interest is a tuple of a name, a thumbnail URL and a relevance score. This
// class is one-shot. To retrieve interests multiple times use multiple
// instances of this class.
//
// Authentication is done via OAuth2 and requires the
// https://www.googleapis.com/auth/googlenow scope.
//
// If the InterestsFetcher is deleted before the callback is called then the
// request will be aborted. It is up to the user to ensure that both the
// Profile and the OAuth2TokenService outlive the InterestsFetcher.
//
// The server required for this feature is not publicly available yet.
class InterestsFetcher : public net::URLFetcherDelegate,
                         public OAuth2TokenService::Consumer {
 public:
  struct Interest {
    Interest(const std::string& name, const GURL& image_url, double relevance);
    ~Interest();

    bool operator==(const Interest& interest) const;

    std::string name;
    GURL image_url;
    double relevance;
  };

  using InterestsCallback =
      base::Callback<void(std::unique_ptr<std::vector<Interest>>)>;

  InterestsFetcher(OAuth2TokenService* oauth2_token_service,
                   const std::string& account_id,
                   net::URLRequestContextGetter* context);

  ~InterestsFetcher() override;

  static std::unique_ptr<InterestsFetcher> CreateFromProfile(Profile* profile);

  void FetchInterests(const InterestsCallback& callback);

 private:
  // net::URLFetcherDelegate implementation.
  void OnURLFetchComplete(const net::URLFetcher* source) override;

  // OAuth2TokenService::Consumer implementation.
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& experiation_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  void StartOAuth2Request();
  OAuth2TokenService::ScopeSet GetApiScopes();
  std::unique_ptr<net::URLFetcher> CreateFetcher();

  // Parse the json response.
  std::unique_ptr<std::vector<Interest>> ExtractInterests(
      const std::string& response);

  InterestsCallback callback_;
  std::unique_ptr<net::URLFetcher> fetcher_;
  std::string account_id_;
  net::URLRequestContextGetter* url_request_context_;
  bool access_token_expired_;
  std::string access_token_;

  std::unique_ptr<OAuth2TokenService::Request> oauth_request_;
  OAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(InterestsFetcher);
};

#endif  // CHROME_BROWSER_INTERESTS_INTERESTS_FETCHER_H_
