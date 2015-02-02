// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/common/password_manager_pref_names.h"

namespace password_manager {
namespace prefs {

const char kAllowToCollectURLBubbleWasShown[] =
    "password_manager_url_collection_bubble.appearance_flag";
const char kAllowToCollectURLBubbleActivePeriodStartFactor[] =
    "password_manager_url_collection_bubble.active_period_start_id";

#if !defined(OS_MACOSX) && !defined(OS_CHROMEOS) && defined(OS_POSIX)
const char kLocalProfileId[] = "profile.local_profile_id";
#endif

#if defined(OS_WIN)
const char kOsPasswordBlank[] = "password_manager.os_password_blank";
const char kOsPasswordLastChanged[] =
    "password_manager.os_password_last_changed";
#endif

const char kPasswordManagerAllowShowPasswords[] =
    "profile.password_manager_allow_show_passwords";
const char kPasswordManagerAutoSignin[] =
    "profile.password_manager_auto_signin";
const char kPasswordManagerSavingEnabled[] = "profile.password_manager_enabled";
const char kPasswordManagerGroupsForDomains[] =
    "profile.password_manager_groups_for_domains";

}  // namespace prefs
}  // namespace password_manager
