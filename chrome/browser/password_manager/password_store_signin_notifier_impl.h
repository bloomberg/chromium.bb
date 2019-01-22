// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_

#include "base/macros.h"

#include "components/password_manager/core/browser/password_store_signin_notifier.h"
#include "services/identity/public/cpp/identity_manager.h"

class Profile;

namespace password_manager {

// Responsible for subscribing to Chrome sign-in events and passing them to
// PasswordStore.
class PasswordStoreSigninNotifierImpl
    : public PasswordStoreSigninNotifier,
      public identity::IdentityManager::Observer {
 public:
  explicit PasswordStoreSigninNotifierImpl(Profile* profile);
  ~PasswordStoreSigninNotifierImpl() override;

  // PasswordStoreSigninNotifier implementations.
  void SubscribeToSigninEvents(PasswordStore* store) override;
  void UnsubscribeFromSigninEvents() override;

  // SigninManagerBase::Observer implementations.
  void OnPrimaryAccountSetWithPassword(const AccountInfo& account_info,
                                       const std::string& password) override;
  void OnPrimaryAccountCleared(const AccountInfo& account_info) override;

  // IdentityManager::Observer implementations.
  void OnAccountRemovedWithInfo(const AccountInfo& info) override;

 private:
  Profile* const profile_;
};

}  // namespace password_manager
#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_STORE_SIGNIN_NOTIFIER_IMPL_H_
