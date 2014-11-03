// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_switches.h"

namespace password_manager {

namespace switches {

// Force the password manager to allow sync credentials to be autofilled.
const char kAllowAutofillSyncCredential[] =
    "allow-autofill-sync-credential";

// Disable dropping the credential used to sync passwords.
const char kDisableDropSyncCredential[] =
    "disable-drop-sync-credential";

// Disable both saving and filling for the sync signin form.
const char kDisableManagerForSyncSignin[] =
    "disable-manager-for-sync-signin";

// Disable the link in the password manager settings page that points to account
// central.
const char kDisablePasswordLink[] = "disable-password-link";

// Disallow autofilling of the sync credential.
const char kDisallowAutofillSyncCredential[] =
    "disallow-autofill-sync-credential";

// Disallow autofilling of the sync credential only for transactional reauth
// pages.
const char kDisallowAutofillSyncCredentialForReauth[] =
    "disallow-autofill-sync-credential-for-reauth";

// Disables the save-password prompt. Passwords are then saved automatically,
// without asking the user.
const char kEnableAutomaticPasswordSaving[] =
    "enable-automatic-password-saving";

// Enable dropping the credential used to sync passwords.
const char kEnableDropSyncCredential[] =
    "enable-drop-sync-credential";

// Enable saving and filling for the sync signin form. Currently the default
// behavior.
const char kEnableManagerForSyncSignin[] =
    "enable-manager-for-sync-signin";

// Enable the link in the password manager settings page that points to account
// central.
const char kEnablePasswordLink[] = "enable-password-link";

}  // namespace switches

}  // namespace password_manager
