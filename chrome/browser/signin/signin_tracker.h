// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_
#define CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_

#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_types.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;

// The signin flow logic is spread across several classes with varying
// responsibilities:
//
// SigninTracker (this class) - This class listens to notifications from various
// services (SigninManager, ProfileSyncService, etc) and coalesces them into
// notifications for the UI layer. This is the class that encapsulates the logic
// that determines whether a user is fully logged in or not, and exposes
// callbacks to SyncSetupHandler (login failed, login succeeded, services
// started up) to help drive the login wizard.
//
// SyncSetupHandler - This class is primarily responsible for interacting with
// the web UI for performing system login and sync configuration. Receives
// callbacks from the UI when the user wishes to initiate a login, and
// translates system state (login errors, etc) into the appropriate calls into
// the UI to reflect this status to the user. Various subclasses
// (OptionsSyncSetupHandler and SyncPromoHandler provide different UIs to the
// user, but the core logic lies in the base SyncSetupHandler class).
//
// LoginUIService - Our desktop UI flows rely on having only a single login flow
// visible to the user at once. This is achieved via LoginUIService (a
// ProfileKeyedService that keeps track of the currently visible login UI).
//
// SigninManager - Records the currently-logged-in user and handles all
// interaction with the GAIA backend during the signin process. Unlike
// SigninTracker, SigninManager only knows about the GAIA login state and is
// not aware of the state of any signed in services.
//
// TokenService - Uses credentials provided by SigninManager to generate tokens
// for all signed-in services in Chrome.
//
// ProfileSyncService - Provides the external API for interacting with the
// sync framework. Listens for notifications from the TokenService to know
// when to startup sync, and provides an Observer interface to notify the UI
// layer of changes in sync state so they can be reflected in the UI.
class SigninTracker : public ProfileSyncServiceObserver,
                      public content::NotificationObserver {
 public:
  class Observer {
   public:
    // The GAIA credentials entered by the user have been validated.
    virtual void GaiaCredentialsValid() = 0;

    // The signin attempt failed. If this is called after GaiaCredentialsValid()
    // then it means there was an error launching one of the dependent services.
    virtual void SigninFailed(const GoogleServiceAuthError& error) = 0;

    // The signin attempt succeeded.
    virtual void SigninSuccess() = 0;
  };

  // The various states the login process can be in.
  enum LoginState {
    WAITING_FOR_GAIA_VALIDATION,
    SERVICES_INITIALIZING,
    SIGNIN_COMPLETE
  };

  // Creates a SigninTracker that tracks the signin status on the passed
  // |profile|, and notifies the |observer| on status changes. |observer| must
  // be non-null and must outlive the SigninTracker.
  SigninTracker(Profile* profile, Observer* observer);
  SigninTracker(Profile* profile, Observer* observer, LoginState state);
  virtual ~SigninTracker();

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // ProfileSyncServiceObserver implementation.
  virtual void OnStateChanged() OVERRIDE;

  // Returns true if the various authenticated services are properly signed in
  // (all tokens loaded, no auth/startup errors, etc).
  static bool AreServicesSignedIn(Profile* profile);

  // Returns true if the tokens are loaded for all signed-in services.
  static bool AreServiceTokensLoaded(Profile* profile);

 private:
  // Initializes this by adding notifications and observers.
  void Initialize();

  // Invoked when one of the services potentially changed its signin status so
  // we can check to see whether we need to notify our observer.
  void HandleServiceStateChange();

  // The current state of the login process.
  LoginState state_;

  // The profile whose signin status we are tracking.
  Profile* profile_;

  // Weak pointer to the observer we call when the signin state changes.
  Observer* observer_;

  // Set to true when SigninManager has validated our credentials.
  bool credentials_valid_;

  // Used to listen to notifications from the SigninManager.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(SigninTracker);
};

#endif  // CHROME_BROWSER_SIGNIN_SIGNIN_TRACKER_H_
