// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/arc/arc_auth_context.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

namespace arc {

// The instance is not reusable, so for each Fetch(), the instance must be
// re-created. Deleting the instance cancels inflight operation.
class ArcAuthCodeFetcher : public OAuth2TokenService::Consumer,
                           public net::URLFetcherDelegate {
 public:
  ArcAuthCodeFetcher(Profile* profile, ArcAuthContext* context);
  ~ArcAuthCodeFetcher() override;

  // Starts to fetch the token. On success fetched |auth_token| is passed.
  // On error, auth_token is empty.
  using FetchCallback = base::Callback<void(const std::string& auth_token)>;
  void Fetch(const FetchCallback& callback);

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
  void OnPrepared(net::URLRequestContextGetter* request_context_getter);

  // Unowned pointers.
  Profile* const profile_;
  ArcAuthContext* const context_;
  net::URLRequestContextGetter* request_context_getter_ = nullptr;

  FetchCallback callback_;

  std::unique_ptr<OAuth2TokenService::Request> login_token_request_;
  std::unique_ptr<net::URLFetcher> auth_code_fetcher_;

  base::WeakPtrFactory<ArcAuthCodeFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAuthCodeFetcher);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_ARC_AUTH_CODE_FETCHER_H_
