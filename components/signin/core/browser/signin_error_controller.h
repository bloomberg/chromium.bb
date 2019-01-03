// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_

#include <set>
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

// Keep track of auth errors and expose them to observers in the UI. Services
// that wish to expose auth errors to the user should register an
// AuthStatusProvider to report their current authentication state, and should
// invoke AuthStatusChanged() when their authentication state may have changed.
class SigninErrorController : public KeyedService,
                              public OAuth2TokenService::Observer,
                              public SigninManagerBase::Observer {
 public:
  enum class AccountMode {
    // Signin error controller monitors all the accounts. When multiple accounts
    // are in error state, only one of the errors is reported.
    ANY_ACCOUNT,

    // Only errors on the primary account are reported. Other accounts are
    // ignored.
    PRIMARY_ACCOUNT
  };

  // The observer class for SigninErrorController lets the controller notify
  // observers when an error arises or changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnErrorChanged() = 0;
  };

  SigninErrorController(AccountMode mode,
                        OAuth2TokenService* token_service,
                        SigninManagerBase* signin_manager);
  ~SigninErrorController() override;

  // KeyedService implementation:
  void Shutdown() override;

  // True if there exists an error worth elevating to the user.
  bool HasError() const;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  const std::string& error_account_id() const { return error_account_id_; }
  const GoogleServiceAuthError& auth_error() const { return auth_error_; }

 private:
  // Invoked when the auth status has changed.
  void Update();

  // OAuth2TokenService::Observer implementation:
  void OnEndBatchChanges() override;
  void OnAuthErrorChanged(const std::string& account_id,
                          const GoogleServiceAuthError& auth_error) override;

  // SigninManagerBase::Observer implementation:
  void GoogleSigninSucceeded(const AccountInfo& account_info) override;
  void GoogleSignedOut(const AccountInfo& account_info) override;

  const AccountMode account_mode_;
  OAuth2TokenService* token_service_;

  // Used only in PRIMARY_ACCOUNT mode, where it must be non-null; otherwise,
  // may be null.
  SigninManagerBase* signin_manager_;
  ScopedObserver<OAuth2TokenService, SigninErrorController>
      scoped_token_service_observer_;
  ScopedObserver<SigninManagerBase, SigninErrorController>
      scoped_signin_manager_observer_;

  // The account that generated the last auth error.
  std::string error_account_id_;

  // The auth error detected the last time AuthStatusChanged() was invoked (or
  // NONE if AuthStatusChanged() has never been invoked).
  GoogleServiceAuthError auth_error_;

  base::ObserverList<Observer, false>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SigninErrorController);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_ERROR_CONTROLLER_H_
