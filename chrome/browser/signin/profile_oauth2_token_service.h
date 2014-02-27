// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
#define CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/linked_ptr.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace net {
class URLRequestContextGetter;
}

class GoogleServiceAuthError;
class Profile;
class SigninGlobalError;

// ProfileOAuth2TokenService is a class that retrieves
// OAuth2 access tokens for a given set of scopes using the OAuth2 login
// refresh tokens.
//
// See |OAuth2TokenService| for usage details.
//
// Note: after StartRequest returns, in-flight requests will continue
// even if the TokenService refresh token that was used to initiate
// the request changes or is cleared.  When the request completes,
// Consumer::OnGetTokenSuccess will be invoked, but the access token
// won't be cached.
//
// Note: requests should be started from the UI thread. To start a
// request from other thread, please use ProfileOAuth2TokenServiceRequest.
class ProfileOAuth2TokenService : public OAuth2TokenService {
 public:
  virtual ~ProfileOAuth2TokenService();

  // Initializes this token service with the profile.
  virtual void Initialize(Profile* profile);

  virtual void Shutdown();

  // Gets an account id of the primary account related to the profile.
  // DEPRECATED: Use SigninManagerBase::GetAuthenticatedAccountId() instead.
  std::string GetPrimaryAccountId();

  // Lists account IDs of all accounts with a refresh token.
  virtual std::vector<std::string> GetAccounts() OVERRIDE;

  // Loads credentials from a backing persistent store to make them available
  // after service is used between profile restarts.
  //
  // Only call this method if there is at least one account connected to the
  // profile, otherwise startup will cause unneeded work on the IO thread.  The
  // primary account is specified with the |primary_account_id| argument and
  // should not be empty.  For a regular profile, the primary account id comes
  // from SigninManager.  For a managed account, the id comes from
  // ManagedUserService.
  virtual void LoadCredentials(const std::string& primary_account_id);

  // Updates a |refresh_token| for an |account_id|. Credentials are persisted,
  // and available through |LoadCredentials| after service is restarted.
  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token);

  // Revokes all credentials handled by the object.
  virtual void RevokeAllCredentials();

  SigninGlobalError* signin_global_error() {
    return signin_global_error_.get();
  }

  const SigninGlobalError* signin_global_error() const {
    return signin_global_error_.get();
  }

  Profile* profile() const { return profile_; }

 protected:
  ProfileOAuth2TokenService();

  // OAuth2TokenService overrides.
  // Note: These methods are overriden so that ProfileOAuth2TokenService is a
  // concrete class.

  // Simply returns NULL and should be overriden by subsclasses.
  virtual net::URLRequestContextGetter* GetRequestContext() OVERRIDE;

  // Default implementation of this method is NOTREACHED as it should be be
  // overriden by subclasses.
  virtual std::string GetRefreshToken(const std::string& account_id)
      const OVERRIDE;

  // Updates the internal cache of the result from the most-recently-completed
  // auth request (used for reporting errors to the user).
  virtual void UpdateAuthError(
      const std::string& account_id,
      const GoogleServiceAuthError& error) OVERRIDE;

 private:
  // The profile with which this instance was initialized, or NULL.
  Profile* profile_;

  // Used to show auth errors in the wrench menu. The SigninGlobalError is
  // different than most GlobalErrors in that its lifetime is controlled by
  // ProfileOAuth2TokenService (so we can expose a reference for use in the
  // wrench menu).
  scoped_ptr<SigninGlobalError> signin_global_error_;

  DISALLOW_COPY_AND_ASSIGN(ProfileOAuth2TokenService);
};

#endif  // CHROME_BROWSER_SIGNIN_PROFILE_OAUTH2_TOKEN_SERVICE_H_
