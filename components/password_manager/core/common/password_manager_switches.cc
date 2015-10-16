// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_switches.h"

namespace password_manager {

namespace switches {

// Force the password manager to allow sync credentials to be autofilled.
const char kAllowAutofillSyncCredential[] = "allow-autofill-sync-credential";

// Disable affiliation based matching, so that credentials stored for an Android
// application will not be considered matches for, and will not be filled into
// corresponding Web applications.
const char kDisableAffiliationBasedMatching[] =
    "disable-affiliation-based-matching";

// Disable dropping the credential used to sync passwords.
const char kDisableDropSyncCredential[] = "disable-drop-sync-credential";

// Disable both saving and filling for the sync signin form.
const char kDisableManagerForSyncSignin[] = "disable-manager-for-sync-signin";

// Disallow autofilling of the sync credential.
const char kDisallowAutofillSyncCredential[] =
    "disallow-autofill-sync-credential";

// Disallow autofilling of the sync credential only for transactional reauth
// pages.
const char kDisallowAutofillSyncCredentialForReauth[] =
    "disallow-autofill-sync-credential-for-reauth";

// Enable affiliation based matching, so that credentials stored for an Android
// application will also be considered matches for, and be filled into
// corresponding Web applications.
const char kEnableAffiliationBasedMatching[] =
    "enable-affiliation-based-matching";

// Disables the save-password prompt. Passwords are then saved automatically,
// without asking the user.
const char kEnableAutomaticPasswordSaving[] =
    "enable-automatic-password-saving";

// Enable dropping the credential used to sync passwords.
const char kEnableDropSyncCredential[] = "enable-drop-sync-credential";

// Enable saving and filling for the sync signin form. Currently the default
// behavior.
const char kEnableManagerForSyncSignin[] = "enable-manager-for-sync-signin";

// Enable supporting of updating password in the password manager on a password
// change form submit.
const char kEnablePasswordChangeSupport[] = "enable-password-change-support";

// Enable a context menu item in the password field that allows the user
// to manually enforce saving of their password.
const char kEnablePasswordForceSaving[] = "enable-password-force-saving";

}  // namespace switches

}  // namespace password_manager
