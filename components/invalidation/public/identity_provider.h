// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
#define COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "google_apis/gaia/oauth2_token_service.h"

namespace invalidation {

// Helper class that provides access to information about the "active GAIA
// account" with which invalidation should interact. The definition of the
// "active Gaia account is context-dependent": the purpose of this abstraction
// layer is to allow invalidation to interact with either device identity or
// user identity via a uniform interface.
class IdentityProvider : public OAuth2TokenService::Observer {
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

   protected:
    virtual ~Observer();
  };

  ~IdentityProvider() override;

  // Adds and removes observers that will be notified of changes to the refresh
  // token availability for the active account.
  void AddActiveAccountRefreshTokenObserver(
      OAuth2TokenService::Observer* observer);
  void RemoveActiveAccountRefreshTokenObserver(
      OAuth2TokenService::Observer* observer);

  // Gets the active account's account ID.
  virtual std::string GetActiveAccountId() = 0;

  // Returns true iff (1) there is an active account and (2) that account has
  // a refresh token.
  virtual bool IsActiveAccountAvailable() = 0;

  // DEPRECATED: Do not add further usage of this API, as it is in the process
  // of being removed. See https://crbug.com/809452.
  // Gets the token service vending OAuth tokens for all logged-in accounts.
  virtual OAuth2TokenService* GetTokenService() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // OAuth2TokenService::Observer:
  void OnRefreshTokenAvailable(const std::string& account_id) override;
  void OnRefreshTokenRevoked(const std::string& account_id) override;
  void OnRefreshTokensLoaded() override;

 protected:
  IdentityProvider();

  // Fires an OnActiveAccountLogin notification.
  void FireOnActiveAccountLogin();

  // Fires an OnActiveAccountLogout notification.
  void FireOnActiveAccountLogout();

 private:
  base::ObserverList<Observer, true> observers_;
  base::ObserverList<OAuth2TokenService::Observer, true>
      token_service_observers_;
  int token_service_observer_count_;

  DISALLOW_COPY_AND_ASSIGN(IdentityProvider);
};

}  // namespace invalidation

#endif  // COMPONENTS_INVALIDATION_PUBLIC_IDENTITY_PROVIDER_H_
