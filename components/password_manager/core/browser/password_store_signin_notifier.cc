// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_signin_notifier.h"

#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store.h"

namespace password_manager {

PasswordStoreSigninNotifier::PasswordStoreSigninNotifier() {}

PasswordStoreSigninNotifier::~PasswordStoreSigninNotifier() {}

void PasswordStoreSigninNotifier::NotifySignin(const std::string& password) {
  metrics_util::LogSyncPasswordHashChange(
      metrics_util::SyncPasswordHashChange::SAVED_ON_CHROME_SIGNIN);
  if (store_)
    store_->SaveSyncPasswordHash(base::UTF8ToUTF16(password));
}

void PasswordStoreSigninNotifier::NotifySignedOut() {
  metrics_util::LogSyncPasswordHashChange(
      metrics_util::SyncPasswordHashChange::CLEARED_ON_CHROME_SIGNOUT);
  if (store_)
    store_->ClearSyncPasswordHash();
}

}  // namespace password_manager
