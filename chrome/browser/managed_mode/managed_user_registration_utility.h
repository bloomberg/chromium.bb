// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "base/strings/string16.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_observer.h"
#include "chrome/browser/managed_mode/managed_users.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"

class GoogleServiceAuthError;
class ManagedUserRefreshTokenFetcher;
class ManagedUserRegistrationUtilityTest;
class PrefService;

namespace browser_sync {
class DeviceInfo;
}

// Structure to store registration information.
struct ManagedUserRegistrationInfo {
  explicit ManagedUserRegistrationInfo(const string16& name);
  string16 name;
  std::string master_key;
};

// Holds the state necessary for registering a new managed user with the
// management server and associating it with its custodian. Each instance
// of this class handles registering a single managed user and should not
// be used afterwards.
class ManagedUserRegistrationUtility
    : public ManagedUserSyncServiceObserver {
 public:
  // Callback for Register() below. If registration is successful, |token| will
  // contain an OAuth2 refresh token for the newly registered managed user,
  // otherwise |token| will be empty and |error| will contain the authentication
  // error for the custodian.
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* token */)>
      RegistrationCallback;

  virtual ~ManagedUserRegistrationUtility();

  static scoped_ptr<ManagedUserRegistrationUtility> Create(Profile* profile);

  static std::string GenerateNewManagedUserId();

  // Registers a new managed user with the server. |managed_user_id| is a new
  // unique ID for the new managed user. If its value is the same as that of
  // of one of the existing managed users, then the same user will be created
  // on this machine. |info| contains necessary information like the display
  // name of the  the user. |callback| is called with the result of the
  // registration. We use the info here and not the profile, because on
  // Chrome OS the profile of the managed user does not yet exist.
  void Register(const std::string& managed_user_id,
                const ManagedUserRegistrationInfo& info,
                const RegistrationCallback& callback);

  // ManagedUserSyncServiceObserver:
  virtual void OnManagedUserAcknowledged(const std::string& managed_user_id)
      OVERRIDE;
  virtual void OnManagedUsersSyncingStopped() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(ManagedUserRegistrationUtilityTest, Register);
  FRIEND_TEST_ALL_PREFIXES(ManagedUserRegistrationUtilityTest,
                           RegisterBeforeInitialSync);
  FRIEND_TEST_ALL_PREFIXES(ManagedUserRegistrationUtilityTest,
                           SyncServiceShutdownBeforeRegFinish);
  FRIEND_TEST_ALL_PREFIXES(ManagedUserRegistrationUtilityTest,
                           StopSyncingBeforeRegFinish);

  // Use the |Create(...)| method to get instances of this class.
  ManagedUserRegistrationUtility(
      PrefService* prefs,
      scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
      ManagedUserSyncService* service);
  // Fetches the managed user token when we have the device name.
  void FetchToken(const std::string& client_name);

  // Called when we have received a token for the managed user.
  void OnReceivedToken(const GoogleServiceAuthError& error,
                       const std::string& token);

  // Dispatches the callback and cleans up if all the conditions have been met.
  void CompleteRegistrationIfReady();

  // Aborts any registration currently in progress. If |run_callback| is true,
  // calls the callback specified in Register() with the given |error|.
  void AbortPendingRegistration(bool run_callback,
                                const GoogleServiceAuthError& error);

  // If |run_callback| is true, dispatches the callback with the saved token
  // (which may be empty) and the given |error|. In any case, resets internal
  // variables to be ready for the next registration.
  void CompleteRegistration(bool run_callback,
                            const GoogleServiceAuthError& error);

  // Cancels any registration currently in progress, without calling the
  // callback or reporting an error.
  void CancelPendingRegistration();

  base::WeakPtrFactory<ManagedUserRegistrationUtility> weak_ptr_factory_;
  PrefService* prefs_;
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher_;

  // A |BrowserContextKeyedService| owned by the custodian profile.
  ManagedUserSyncService* managed_user_sync_service_;

  std::string pending_managed_user_id_;
  std::string pending_managed_user_token_;
  bool pending_managed_user_acknowledged_;
  bool is_existing_managed_user_;
  RegistrationCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserRegistrationUtility);
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_
