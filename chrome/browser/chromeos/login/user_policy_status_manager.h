// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_USER_POLICY_STATUS_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_USER_POLICY_STATUS_MANAGER_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"

class PrefRegistrySimple;
class Profile;

namespace chromeos {

// UserPolicyStatusManager watches the user policy status and manages the flag
// of whether a user has a policy. It also enforces that only the primary
// user (i.e. the first signed-in user) is allowed to have policy by shutting
// down the current session when the rule is broken.
class UserPolicyStatusManager {
 public:
  enum Status {
    USER_POLICY_STATUS_UNKNOWN = 0,
    USER_POLICY_STATUS_NONE = 1,
    USER_POLICY_STATUS_MANAGED = 2
  };

  UserPolicyStatusManager();
  ~UserPolicyStatusManager();

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static Status Get(const std::string& user_email);
  static void Set(const std::string& user_email, Status status);

  void StartObserving(const std::string& user_email, Profile* user_profile);

 private:
  // A helper class to observer user policy store.
  class StoreObserver;

  // Checks if the user identified by |user_email| is supposed to have a policy.
  // Shutdown the current session if the user should not have a policy.
  void CheckUserPolicyAllowed(const std::string& user_email);

  std::string primary_user_email_;
  ScopedVector<StoreObserver> store_observers_;

  DISALLOW_COPY_AND_ASSIGN(UserPolicyStatusManager);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_USER_POLICY_STATUS_MANAGER_H_
