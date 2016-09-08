// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace arc {

class ArcAuthCodeFetcherDelegate;

class ArcAuthCodeFetcher : public OAuth2TokenService::Consumer,
                           public net::URLFetcherDelegate {
 public:
  ArcAuthCodeFetcher(ArcAuthCodeFetcherDelegate* delegate,
                     net::URLRequestContextGetter* request_context_getter,
                     Profile* profile,
                     const std::string& auth_endpoint);
  ~ArcAuthCodeFetcher() override;

  // OAuth2TokenService::Consumer:
  void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                         const std::string& access_token,
                         const base::Time& expiration_time) override;
  void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                         const GoogleServiceAuthError& error) override;

  // net::URLFetcherDelegate:
  void OnURLFetchComplete(const net::URLFetcher* source) override;

 private:
  void ResetFetchers();

  // Unowned pointers.
  ArcAuthCodeFetcherDelegate* const delegate_;
  net::URLRequestContextGetter* const request_context_getter_;
  Profile* const profile_;

  // URL to request auth code.
  const std::string auth_endpoint_;

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;
  std::unique_ptr<net::URLFetcher> auth_code_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_
