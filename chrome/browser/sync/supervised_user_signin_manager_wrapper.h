// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SUPERVISED_USER_SIGNIN_MANAGER_WRAPPER_H_
#define CHROME_BROWSER_SYNC_SUPERVISED_USER_SIGNIN_MANAGER_WRAPPER_H_

#include <string>

#include "base/macros.h"
#include "components/sync/driver/signin_manager_wrapper.h"

class Profile;
class SigninManagerBase;

namespace identity {
class IdentityManager;
}

// Some chrome cloud services support supervised users as well as normally
// authenticated users that sign in through SigninManager.  To facilitate
// getting the "effective" username and account identifiers, services can
// use this class to wrap the SigninManager and return supervised user account
// information when appropriate.
class SupervisedUserSigninManagerWrapper : public SigninManagerWrapper {
 public:
  SupervisedUserSigninManagerWrapper(
      Profile* profile,
      identity::IdentityManager* identity_manager,
      SigninManagerBase* signin_manager);
  ~SupervisedUserSigninManagerWrapper() override;

  // SigninManagerWrapper implementation
  std::string GetEffectiveUsername() const override;
  std::string GetAccountIdToUse() const override;
  std::string GetSyncScopeToUse() const override;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserSigninManagerWrapper);
};

#endif  // CHROME_BROWSER_SYNC_SUPERVISED_USER_SIGNIN_MANAGER_WRAPPER_H_
