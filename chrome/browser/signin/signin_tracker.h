// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

// The signin flow logic is spread across several classes with varying
// responsibilities:
//
// SigninTracker (this class) - This class listens to notifications from various
// services (SigninManager, OAuth2TokenService) and coalesces them into
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
// (a BrowserContextKeyedService that keeps track of the currently visible
// login UI).
//
// SigninManager - Records the currently-logged-in user and handles all
// interaction with the GAIA backend during the signin process. Unlike
// SigninTracker, SigninManager only knows about the GAIA login state and is
// not aware of the state of any signed in services.
//
// TokenService - Uses credentials provided by SigninManager to generate tokens
// for all signed-in services in Chrome.
//
// OAuth2TokenService - Maintains and manages OAuth2 tokens for the accounts
// connected to this profile.
//
// ProfileSyncService - Provides the external API for interacting with the
// sync framework. Listens for notifications from the TokenService to know
// when to startup sync, and provides an Observer interface to notify the UI
// layer of changes in sync state so they can be reflected in the UI.
class SigninTracker : public content::NotificationObserver,
                      public OAuth2TokenService::Observer {
 public:
  class Observer {
   public:
    // The signin attempt failed, and the cause is passed in |error|.
    virtual void SigninFailed(const GoogleServiceAuthError& error) = 0;

    // The signin attempt succeeded.
    virtual void SigninSuccess() = 0;
  };

  // Creates a SigninTracker that tracks the signin status on the passed
  // |profile|, and notifies the |observer| on status changes. |observer| must
  // be non-null and must outlive the SigninTracker.
  SigninTracker(Profile* profile, Observer* observer);
  virtual ~SigninTracker();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // OAuth2TokenService::Observer implementation.
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;

 private:
  // Initializes this by adding notifications and observers.
  void Initialize();

  // The profile whose signin status we are tracking.
  Profile* profile_;

  // Weak pointer to the observer we call when the signin state changes.
  Observer* observer_;

  // Used to listen to notifications from the SigninManager.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SigninTracker);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_
