// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_APIARY_H_
#define CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_APIARY_H_

#include "chrome/browser/supervised_user/permission_request_creator.h"

#include "base/memory/scoped_ptr.h"
#include "google_apis/gaia/oauth2_token_service.h"
#include "net/url_request/url_fetcher_delegate.h"

class Profile;
class SupervisedUserSigninManagerWrapper;

namespace base {
class Time;
}

namespace net {
class URLFetcher;
class URLRequestContextGetter;
}

class PermissionRequestCreatorApiary : public PermissionRequestCreator,
                                       public OAuth2TokenService::Consumer,
                                       public net::URLFetcherDelegate {
 public:
  PermissionRequestCreatorApiary(
      OAuth2TokenService* oauth2_token_service,
      scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper,
      net::URLRequestContextGetter* context);
  virtual ~PermissionRequestCreatorApiary();

  static scoped_ptr<PermissionRequestCreator> CreateWithProfile(
      Profile* profile);

  // PermissionRequestCreator implementation:
  virtual void CreatePermissionRequest(const std::string& url_requested,
                                       const base::Closure& callback) OVERRIDE;

 private:
  // OAuth2TokenService::Consumer implementation:
  virtual void OnGetTokenSuccess(const OAuth2TokenService::Request* request,
                                 const std::string& access_token,
                                 const base::Time& expiration_time) OVERRIDE;
  virtual void OnGetTokenFailure(const OAuth2TokenService::Request* request,
                                 const GoogleServiceAuthError& error) OVERRIDE;

  // net::URLFetcherDelegate implementation.
  virtual void OnURLFetchComplete(const net::URLFetcher* source) OVERRIDE;

  // Requests an access token, which is the first thing we need. This is where
  // we restart when the returned access token has expired.
  void StartFetching();

  void DispatchNetworkError(int error_code);
  void DispatchGoogleServiceAuthError(const GoogleServiceAuthError& error);

  OAuth2TokenService* oauth2_token_service_;
  scoped_ptr<SupervisedUserSigninManagerWrapper> signin_wrapper_;
  base::Closure callback_;
  net::URLRequestContextGetter* context_;
  std::string url_requested_;
  scoped_ptr<OAuth2TokenService::Request> access_token_request_;
  std::string access_token_;
  bool access_token_expired_;
  scoped_ptr<net::URLFetcher> url_fetcher_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_PERMISSION_REQUEST_CREATOR_APIARY_H_
