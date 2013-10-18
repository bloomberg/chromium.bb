// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_USER_MANAGER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_USER_MANAGER_IMPL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/login/supervised_user_manager.h"

namespace chromeos {

class CrosSettings;
class UserManagerImpl;

// Implementation of the UserManager.
class SupervisedUserManagerImpl
    : public SupervisedUserManager {
 public:
  virtual ~SupervisedUserManagerImpl();

  virtual const User* CreateUserRecord(
      const std::string& manager_id,
      const std::string& local_user_id,
      const std::string& sync_user_id,
      const string16& display_name) OVERRIDE;
  virtual std::string GenerateUserId() OVERRIDE;
  virtual const User* FindByDisplayName(const string16& display_name) const
      OVERRIDE;
  virtual const User* FindBySyncId(const std::string& sync_id) const OVERRIDE;
  virtual std::string GetUserSyncId(const std::string& user_id) const OVERRIDE;
  virtual string16 GetManagerDisplayName(const std::string& user_id) const
      OVERRIDE;
  virtual std::string GetManagerUserId(const std::string& user_id) const
      OVERRIDE;
  virtual std::string GetManagerDisplayEmail(const std::string& user_id) const
      OVERRIDE;
  virtual void StartCreationTransaction(const string16& display_name) OVERRIDE;
  virtual void SetCreationTransactionUserId(const std::string& user_id)
      OVERRIDE;
  virtual void CommitCreationTransaction() OVERRIDE;

 private:
  friend class UserManager;
  friend class UserManagerImpl;

  explicit SupervisedUserManagerImpl(UserManagerImpl* owner);

  // Returns true if there is non-committed user creation transaction.
  bool HasFailedUserCreationTransaction();

  // Attempts to clean up data that could be left from failed user creation.
  void RollbackUserCreationTransaction();

  void RemoveNonCryptohomeData(const std::string& user_id);

  bool CheckForFirstRun(const std::string& user_id);

  // Update name if this user is manager of some managed users.
  void UpdateManagerName(const std::string& manager_id,
                         const string16& new_display_name);

  UserManagerImpl* owner_;

  // Interface to the signed settings store.
  CrosSettings* cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserManagerImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SUPERVISED_USER_MANAGER_IMPL_H_
