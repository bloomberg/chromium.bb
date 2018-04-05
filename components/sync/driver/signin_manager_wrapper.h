// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SIGNIN_MANAGER_WRAPPER_H_
#define COMPONENTS_SYNC_DRIVER_SIGNIN_MANAGER_WRAPPER_H_

#include <string>

#include "base/macros.h"

class SigninManagerBase;

namespace identity {
class IdentityManager;
}

// Wraps SigninManager so subclasses can support different ways of getting
// account information if necessary. Currently exists for supervised users;
// the subclass SupervisedUserSigninManagerWrapper may be merged back into
// this class once supervised users are componentized.
class SigninManagerWrapper {
 public:
  explicit SigninManagerWrapper(identity::IdentityManager* identity_manager,
                                SigninManagerBase* signin_manager);
  virtual ~SigninManagerWrapper();

  // Get the email address to use for this account.
  virtual std::string GetEffectiveUsername() const;

  // Get the unique ID used to represent this account.
  virtual std::string GetAccountIdToUse() const;

  // Get the OAuth2 scope to use for this account.
  virtual std::string GetSyncScopeToUse() const;

  // Return the original IdentityManager object that was passed in.
  identity::IdentityManager* GetIdentityManager();

  // Return the original SigninManagerBase object that was passed in.
  SigninManagerBase* GetSigninManager();

 private:
  identity::IdentityManager* identity_manager_;
  SigninManagerBase* signin_manager_;

  DISALLOW_COPY_AND_ASSIGN(SigninManagerWrapper);
};

#endif  // COMPONENTS_SYNC_DRIVER_SIGNIN_MANAGER_WRAPPER_H_
