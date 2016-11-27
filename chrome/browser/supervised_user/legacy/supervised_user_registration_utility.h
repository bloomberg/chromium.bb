// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_REGISTRATION_UTILITY_H_
#define CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_REGISTRATION_UTILITY_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_observer.h"
#include "chrome/browser/supervised_user/supervised_users.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"

class GoogleServiceAuthError;
class PrefService;
class Profile;
class SupervisedUserRefreshTokenFetcher;
class SupervisedUserRegistrationUtilityTest;
class SupervisedUserSharedSettingsService;

// Structure to store registration information.
struct SupervisedUserRegistrationInfo {
  SupervisedUserRegistrationInfo(const base::string16& name, int avatar_index);
  ~SupervisedUserRegistrationInfo();
  int avatar_index;
  base::string16 name;
  std::string master_key;
  std::string password_signature_key;
  std::string password_encryption_key;
  base::DictionaryValue password_data;
};

// Holds the state necessary for registering a new supervised user with the
// management server and associating it with its custodian. Each instance
// of this class handles registering a single supervised user and should not
// be used afterwards.
class SupervisedUserRegistrationUtility {
 public:
  // Callback for Register() below. If registration is successful, |token| will
  // contain an OAuth2 refresh token for the newly registered supervised user,
  // otherwise |token| will be empty and |error| will contain the authentication
  // error for the custodian.
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* token */)>
      RegistrationCallback;

  virtual ~SupervisedUserRegistrationUtility() {}

  // Creates SupervisedUserRegistrationUtility for a given |profile|.
  static std::unique_ptr<SupervisedUserRegistrationUtility> Create(
      Profile* profile);

  static std::string GenerateNewSupervisedUserId();

  // Registers a new supervised user with the server. |supervised_user_id| is a
  // new unique ID for the new supervised user. If its value is the same as that
  // of one of the existing supervised users, then the same user will be created
  // on this machine (and if they have no avatar in sync, their avatar will be
  // updated). |info| contains necessary information like the display name of
  // the user and their avatar. |callback| is called with the result of the
  // registration. We use the info here and not the profile, because on Chrome
  // OS the profile of the supervised user does not yet exist.
  virtual void Register(const std::string& supervised_user_id,
                        const SupervisedUserRegistrationInfo& info,
                        const RegistrationCallback& callback) = 0;

 protected:
  SupervisedUserRegistrationUtility() {}

 private:
  friend class ScopedTestingSupervisedUserRegistrationUtility;
  friend class SupervisedUserRegistrationUtilityTest;

  // Creates implementation with explicit dependencies, can be used for testing.
  static SupervisedUserRegistrationUtility* CreateImpl(
      PrefService* prefs,
      std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher,
      SupervisedUserSyncService* service,
      SupervisedUserSharedSettingsService* shared_settings_service);

  // Set the instance of SupervisedUserRegistrationUtility that will be returned
  // by next Create() call. Takes ownership of the |utility|.
  static void SetUtilityForTests(SupervisedUserRegistrationUtility* utility);
};

// Class that sets the instance of SupervisedUserRegistrationUtility that will
// be returned by next Create() call, and correctly destroys it if Create() was
// not called.
class ScopedTestingSupervisedUserRegistrationUtility {
 public:
  // Delegates ownership of the |instance| to SupervisedUserRegistrationUtility.
  ScopedTestingSupervisedUserRegistrationUtility(
      SupervisedUserRegistrationUtility* instance);

  ~ScopedTestingSupervisedUserRegistrationUtility();
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LEGACY_SUPERVISED_USER_REGISTRATION_UTILITY_H_
