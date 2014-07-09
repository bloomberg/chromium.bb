// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_switches.h"

namespace password_manager {

namespace switches {

// Disable both saving and filling for the sync signin form.
const char kDisableManagerForSyncSignin[] =
    "disable-manager-for-sync-signin";

// Enable saving and filling for the sync signin form. Currently the default
// behavior.
const char kEnableManagerForSyncSignin[] =
    "enable-manager-for-sync-signin";

// Disables the save-password prompt. Passwords are then saved automatically,
// without asking the user.
const char kEnableAutomaticPasswordSaving[] =
    "enable-automatic-password-saving";

}  // namespace switches

}  // namespace password_manager
