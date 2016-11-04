// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/sync_global_error_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"

namespace signin_ui_util {

base::string16 GetAuthenticatedUsername(const SigninManagerBase* signin) {
  std::string user_display_name;

  if (signin->IsAuthenticated()) {
    user_display_name = signin->GetAuthenticatedAccountInfo().email;

#if defined(OS_CHROMEOS)
    if (user_manager::UserManager::IsInitialized()) {
      // On CrOS user email is sanitized and then passed to the signin manager.
      // Original email (containing dots) is stored as "display email".
      user_display_name = user_manager::UserManager::Get()->GetUserDisplayEmail(
          AccountId::FromUserEmail(user_display_name));
    }
#endif  // defined(OS_CHROMEOS)
  }

  return base::UTF8ToUTF16(user_display_name);
}

void InitializePrefsForProfile(Profile* profile) {
  if (profile->IsNewProfile()) {
    // Suppresses the upgrade tutorial for a new profile.
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, kUpgradeWelcomeTutorialShowMax + 1);
  }
}

void ShowSigninErrorLearnMorePage(Profile* profile) {
  static const char kSigninErrorLearnMoreUrl[] =
      "https://support.google.com/chrome/answer/1181420?";
  chrome::NavigateParams params(
      profile, GURL(kSigninErrorLearnMoreUrl), ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

std::string GetDisplayEmail(Profile* profile, const std::string& account_id) {
  AccountTrackerService* account_tracker =
      AccountTrackerServiceFactory::GetForProfile(profile);
  std::string email = account_tracker->GetAccountInfo(account_id).email;
  if (email.empty()) {
    DCHECK_EQ(AccountTrackerService::MIGRATION_NOT_STARTED,
              account_tracker->GetMigrationState());
    return account_id;
  }
  return email;
}

}  // namespace signin_ui_util
