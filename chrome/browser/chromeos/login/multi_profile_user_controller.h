// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_USER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_USER_CONTROLLER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"

class PrefChangeRegistrar;
class PrefRegistrySimple;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace chromeos {

class MultiProfileUserControllerDelegate;
class UserManager;

// MultiProfileUserController decides whether a user is allowed to be in a
// multi-profiles session. It caches the multiprofile user behavior pref backed
// by user policy into local state so that the value is available before the
// user login and checks if the meaning of the value is respected.
class MultiProfileUserController {
 public:
  MultiProfileUserController(MultiProfileUserControllerDelegate* delegate,
                             PrefService* local_state);
  ~MultiProfileUserController();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns true if the user is allowed to be in the current session.
  bool IsUserAllowedInSession(const std::string& user_email) const;

  // Starts to observe the multiprofile user behavior pref of the given profile.
  void StartObserving(Profile* user_profile);

  // Removes the cached value for the given user.
  void RemoveCachedValue(const std::string& user_email);

  // Possible behavior values.
  static const char kBehaviorUnrestricted[];
  static const char kBehaviorPrimaryOnly[];
  static const char kBehaviorNotAllowed[];

 private:
  friend class MultiProfileUserControllerTest;

  // Gets/sets the cached policy value.
  std::string GetCachedValue(const std::string& user_email) const;
  void SetCachedValue(const std::string& user_email,
                      const std::string& behavior);

  // Checks if all users are allowed in the current session.
  void CheckSessionUsers();

  // Invoked when user behavior pref value changes.
  void OnUserPrefChanged(Profile* profile);

  MultiProfileUserControllerDelegate* delegate_;  // Not owned.
  PrefService* local_state_;  // Not owned.
  ScopedVector<PrefChangeRegistrar> pref_watchers_;

  DISALLOW_COPY_AND_ASSIGN(MultiProfileUserController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_MULTI_PROFILE_USER_CONTROLLER_H_
