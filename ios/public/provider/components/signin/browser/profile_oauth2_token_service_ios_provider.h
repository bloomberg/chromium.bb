// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_COMPONENTS_SIGNIN_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_COMPONENTS_SIGNIN_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_

#if defined(__OBJC__)
@class NSDate;
@class NSError;
@class NSString;
#else
class NSDate;
class NSError;
class NSString;
#endif  // defined(__OBJC__)

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"

namespace ios {

enum AuthenticationErrorCategory {
  // Unknown errors.
  kAuthenticationErrorCategoryUnknownErrors,
  // Authorization errors.
  kAuthenticationErrorCategoryAuthorizationErrors,
  // Authorization errors with HTTP_FORBIDDEN (403) error code.
  kAuthenticationErrorCategoryAuthorizationForbiddenErrors,
  // Network server errors includes parsing error and should be treated as
  // transient/offline errors.
  kAuthenticationErrorCategoryNetworkServerErrors,
  // User cancellation errors should be handled by treating them as a no-op.
  kAuthenticationErrorCategoryUserCancellationErrors,
  // User identity not found errors.
  kAuthenticationErrorCategoryUnknownIdentityErrors,
};

// Interface that provides support for ProfileOAuth2TokenServiceIOS.
class ProfileOAuth2TokenServiceIOSProvider {
 public:
  typedef base::Callback<void(NSString* token,
                              NSDate* expiration,
                              NSError* error)> AccessTokenCallback;

  ProfileOAuth2TokenServiceIOSProvider() {};
  virtual ~ProfileOAuth2TokenServiceIOSProvider() {};

  // Returns whether authentication is using the shared authentication library.
  virtual bool IsUsingSharedAuthentication() const = 0;

  // Initializes the shared authentication library. This method should be called
  // when loading credentials if the user is signed in to Chrome via the shared
  // authentication library.
  virtual void InitializeSharedAuthentication() = 0;

  // Returns the ids of all accounts.
  virtual std::vector<std::string> GetAllAccountIds() = 0;

  // Starts fetching an access token for the account with id |account_id| with
  // the given |scopes|. Once the token is obtained, |callback| is called.
  virtual void GetAccessToken(const std::string& account_id,
                              const std::string& client_id,
                              const std::string& client_secret,
                              const std::set<std::string>& scopes,
                              const AccessTokenCallback& callback) = 0;

  // Returns the authentication error category of |error|.
  virtual AuthenticationErrorCategory GetAuthenticationErrorCategory(
      NSError* error) const = 0;
};

}  // namespace ios

#endif  // IOS_PUBLIC_PROVIDER_COMPONENTS_SIGNIN_BROWSER_PROFILE_OAUTH2_TOKEN_SERVICE_IOS_PROVIDER_H_
