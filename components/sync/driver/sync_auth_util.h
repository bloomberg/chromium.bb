// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_

#include "components/signin/core/browser/account_info.h"

class GoogleServiceAuthError;

namespace identity {
class IdentityManager;
}  // namespace identity

namespace syncer {

struct SyncAccountInfo {
  SyncAccountInfo();
  SyncAccountInfo(const CoreAccountInfo& account_info, bool is_primary);

  CoreAccountInfo account_info;
  bool is_primary = false;
};

// Determines which account should be used for Sync and returns the
// corresponding SyncAccountInfo. This is exposed so that autofill metrics
// code can use it.
SyncAccountInfo DetermineAccountToUse(
    identity::IdentityManager* identity_manager,
    bool allow_secondary_accounts);

// Returns whether |auth_error| indicates the user has locally signed out of
// content area, rejecting credentials.
bool IsWebSignout(const GoogleServiceAuthError& auth_error);

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_AUTH_UTIL_H_
