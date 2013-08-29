// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_
#define CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_

#include <map>
#include <string>

#include "base/basictypes.h"
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
class ManagedUserRegistrationUtility {
 public:
  // Callback for Register() below. If registration is successful, |token| will
  // contain an OAuth2 refresh token for the newly registered managed user,
  // otherwise |token| will be empty and |error| will contain the authentication
  // error for the custodian.
  typedef base::Callback<void(const GoogleServiceAuthError& /* error */,
                              const std::string& /* token */)>
      RegistrationCallback;

  virtual ~ManagedUserRegistrationUtility() {}

  // Creates ManagedUserRegistrationUtility for a given |profile|.
  static scoped_ptr<ManagedUserRegistrationUtility> Create(Profile* profile);

  static std::string GenerateNewManagedUserId();

  // Registers a new managed user with the server. |managed_user_id| is a new
  // unique ID for the new managed user. If its value is the same as that of
  // of one of the existing managed users, then the same user will be created
  // on this machine. |info| contains necessary information like the display
  // name of the  the user. |callback| is called with the result of the
  // registration. We use the info here and not the profile, because on
  // Chrome OS the profile of the managed user does not yet exist.
  virtual void Register(const std::string& managed_user_id,
                        const ManagedUserRegistrationInfo& info,
                        const RegistrationCallback& callback) = 0;

 protected:
  ManagedUserRegistrationUtility() {}

 private:
  friend class ScopedTestingManagedUserRegistrationUtility;
  friend class ManagedUserRegistrationUtilityTest;

  // Creates implementation with explicit dependencies, can be used for testing.
  static ManagedUserRegistrationUtility* CreateImpl(
      PrefService* prefs,
      scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
      ManagedUserSyncService* service);

  // Set the instance of ManagedUserRegistrationUtility that will be returned
  // by next Create() call. Takes ownership of the |utility|.
  static void SetUtilityForTests(ManagedUserRegistrationUtility* utility);
};

// Class that sets the instance of ManagedUserRegistrationUtility that will be
// returned by next Create() call, and correctly destroys it if Create() was
// not called.
class ScopedTestingManagedUserRegistrationUtility {
 public:
  // Delegates ownership of the |instance| to ManagedUserRegistrationUtility.
  ScopedTestingManagedUserRegistrationUtility(
      ManagedUserRegistrationUtility* instance);

  ~ScopedTestingManagedUserRegistrationUtility();
};

#endif  // CHROME_BROWSER_MANAGED_MODE_MANAGED_USER_REGISTRATION_UTILITY_H_
