// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_signin_notifier.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

PasswordStoreSigninNotifier::PasswordStoreSigninNotifier() {}

PasswordStoreSigninNotifier::~PasswordStoreSigninNotifier() {}

void PasswordStoreSigninNotifier::NotifySignin(const std::string& username,
                                               const std::string& password) {
  // After the full roll out of DICE, |password| may be empty
  // if user clicks "Sync as ..." button in the sign-in promotion bubble.
  if (store_ && !password.empty()) {
    store_->SaveGaiaPasswordHash(
        username, base::UTF8ToUTF16(password),
        metrics_util::SyncPasswordHashChange::SAVED_ON_CHROME_SIGNIN);
  }
}

void PasswordStoreSigninNotifier::NotifySignedOut(const std::string& username,
                                                  bool primary_account) {
  if (!store_)
    return;

  if (primary_account) {
    metrics_util::LogSyncPasswordHashChange(
        metrics_util::SyncPasswordHashChange::CLEARED_ON_CHROME_SIGNOUT);
    store_->ClearAllGaiaPasswordHash();
  } else {
    store_->ClearGaiaPasswordHash(username);
  }
}

}  // namespace password_manager
