// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_

#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace invalidation {

// An opaque object that clients can use to control the lifetime of access
// token requests.
class ActiveAccountAccessTokenFetcher {
 public:
  ActiveAccountAccessTokenFetcher() = default;
  virtual ~ActiveAccountAccessTokenFetcher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ActiveAccountAccessTokenFetcher);
};

using ActiveAccountAccessTokenCallback =
    base::OnceCallback<void(GoogleServiceAuthError error,
                            std::string access_token)>;

// Helper class that provides access to information about the "active GAIA
// account" with which invalidation should interact. The definition of the
// "active Gaia account is context-dependent": the purpose of this abstraction
// layer is to allow invalidation to interact with either device identity or
// user identity via a uniform interface.
class IdentityProvider {
 public:
  class Observer {
   public:
    // Called when a GAIA account logs in and becomes the active account. All
    // account information is available when this method is called and all
    // |IdentityProvider| methods will return valid data.
    virtual void OnActiveAccountLogin() {}

    // Called when the active GAIA account logs out. The account information may
    // have been cleared already when this method is called. The
    // |IdentityProvider| methods may return inconsistent or outdated
    // information if called from within OnLogout().
    virtual void OnActiveAccountLogout() {}

    // Called when the active GAIA account's refresh token is updated.
    virtual void OnActiveAccountRefreshTokenUpdated() {}

    // Called when the active GAIA account's refresh token is removed.
    virtual void OnActiveAccountRefreshTokenRemoved() {}

   protected:
    virtual ~Observer();
  };

  virtual ~IdentityProvider();

  // Gets the active account's account ID.
  virtual std::string GetActiveAccountId() = 0;

  // Returns true iff (1) there is an active account and (2) that account has
  // a refresh token.
  virtual bool IsActiveAccountAvailable() = 0;

  // Starts an access token request for |oauth_consumer_name| and |scopes|. When
  // the request completes, |callback| will be invoked with the access token
  // or error. To cancel the request, destroy the returned TokenFetcher.
  virtual std::unique_ptr<ActiveAccountAccessTokenFetcher> FetchAccessToken(
      const std::string& oauth_consumer_name,
      const OAuth2TokenService::ScopeSet& scopes,
      ActiveAccountAccessTokenCallback callback) = 0;

  // Marks an OAuth2 |access_token| issued for the active account and |scopes|
  // as invalid.
  virtual void InvalidateAccessToken(const OAuth2TokenService::ScopeSet& scopes,
                                     const std::string& access_token) = 0;

  // Set the account id that should be registered for invalidations.
  virtual void SetActiveAccountId(const std::string& account_id) = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  IdentityProvider();

  // Processes a refresh token update, firing the observer callback if
  // |account_id| is the active account.
  void ProcessRefreshTokenUpdateForAccount(const std::string& account_id);

  // Processes a refresh token removal, firing the observer callback if
  // |account_id| is the active account.
  void ProcessRefreshTokenRemovalForAccount(const std::string& account_id);

  // Fires an OnActiveAccountLogin notification.
  void FireOnActiveAccountLogin();

  // Fires an OnActiveAccountLogout notification.
  void FireOnActiveAccountLogout();

 private:
  base::ObserverList<Observer, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(IdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
