// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {
namespace prefs {

const char kCredentialsEnableAutosignin[] = "credentials_enable_autosignin";
const char kCredentialsEnableService[] = "credentials_enable_service";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
const char kLocalProfileId[] = "profile.local_profile_id";
#endif

#if defined(OS_WIN)
const char kOsPasswordBlank[] = "password_manager.os_password_blank";
const char kOsPasswordLastChanged[] =
    "password_manager.os_password_last_changed";
#endif

#if defined(OS_MACOSX)
const char kKeychainMigrationStatus[] = "password_manager.keychain_migration";
#endif

const char kPasswordManagerAllowShowPasswords[] =
    "profile.password_manager_allow_show_passwords";
const char kPasswordManagerSavingEnabled[] = "profile.password_manager_enabled";
const char kPasswordManagerGroupsForDomains[] =
    "profile.password_manager_groups_for_domains";

const char kWasAutoSignInFirstRunExperienceShown[] =
    "profile.was_auto_sign_in_first_run_experience_shown";

const char kWasSavePrompFirstRunExperienceShown[] =
    "profile.was_save_prompt_first_run_experience_shown";

}  // namespace prefs
}  // namespace password_manager
