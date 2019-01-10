// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_TRACKER_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_TRACKER_H_

#include <memory>

#include "base/macros.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "services/identity/public/cpp/identity_manager.h"

// The signin flow logic is spread across several classes with varying
// responsibilities:
//
// SigninTracker (this class) - This class listens to notifications from the
// IdentityManager services and coalesces them into
// notifications for the UI layer. This is the class that encapsulates the logic
// that determines whether a user is fully logged in or not, and exposes
// callbacks so various pieces of the UI (OneClickSyncStarter) can track the
// current startup state.
//
// SyncSetupHandler - This class is primarily responsible for interacting with
// the web UI for performing system login and sync configuration. Receives
// callbacks from the UI when the user wishes to initiate a login, and
// translates system state (login errors, etc) into the appropriate calls into
// the UI to reflect this status to the user.
//
// LoginUIService - Our desktop UI flows rely on having only a single login flow
// visible to the user at once. This is achieved via LoginUIService
// (a KeyedService that keeps track of the currently visible
// login UI).
//
// IdentityManager - Records the currently-logged-in user and handles all
// interaction with the GAIA backend during the signin process. Unlike
// SigninTracker, IdentityManager only knows about the GAIA login state and is
// not aware of the state of any signed in services.
// What is more, IdentityManager also maintains and manages OAuth2 tokens for
// the accounts connected to this profile.
// Last, the IdentityManager is also responsible for adding or removing cookies
// from the cookie jar from the browser process. A single source of information
// about GAIA cookies in the cookie jar that are fetchable via /ListAccounts.
//
// ProfileSyncService - Provides the external API for interacting with the
// sync framework. Listens for notifications for tokens to know when to startup
// sync, and provides an Observer interface to notify the UI layer of changes
// in sync state so they can be reflected in the UI.
class SigninTracker : public identity::IdentityManager::Observer {
 public:
  class Observer {
   public:
    // The signin attempt failed, and the cause is passed in |error|.
    virtual void SigninFailed(const GoogleServiceAuthError& error) = 0;

    // The signin attempt succeeded.
    virtual void SigninSuccess() = 0;

    // The signed in account has been added into the content area cookie jar.
    // This will be called only after a call to SigninSuccess().
    virtual void AccountAddedToCookie(const GoogleServiceAuthError& error) = 0;
  };

  // Creates a SigninTracker that tracks the signin status on the passed
  // classes, and notifies the |observer| on status changes. All of the
  // instances with the exception of |account_reconcilor| must be non-null and
  // must outlive the SigninTracker. |account_reconcilor| will be used if it is
  // non-null.
  SigninTracker(identity::IdentityManager* identity_manager,
                Observer* observer);
  ~SigninTracker() override;

  // identity::IdentityManager::Observer implementation.
  void OnPrimaryAccountSet(const AccountInfo& account_info) override;
  void OnPrimaryAccountSigninFailed(
      const GoogleServiceAuthError& error) override;
  void OnRefreshTokenUpdatedForAccount(
      const AccountInfo& account_info) override;
  void OnAddAccountToCookieCompleted(
      const std::string& account_id,
      const GoogleServiceAuthError& error) override;

 private:
  // Initializes this by adding notifications and observers.
  void Initialize();

  // The classes whose collective signin status we are tracking.
  identity::IdentityManager* identity_manager_;

  // Weak pointer to the observer we call when the signin state changes.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(SigninTracker);
};

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_SIGNIN_TRACKER_H_
