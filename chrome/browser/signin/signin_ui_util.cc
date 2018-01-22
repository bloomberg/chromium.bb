// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/common/pref_names.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/gaia_cookie_manager_service.h"
#include "components/signin/core/browser/profile_management_switches.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/signin/account_consistency_mode_manager.h"
#include "chrome/browser/ui/webui/signin/dice_turn_sync_on_helper.h"
#endif

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
  NavigateParams params(profile, GURL(kSigninErrorLearnMoreUrl),
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
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

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
// TODO(tangltom): Add a unit test for this function.
std::vector<AccountInfo> GetAccountsForDicePromos(Profile* profile) {
  // Fetch account ids for accounts that have a token.
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  std::vector<std::string> account_ids = token_service->GetAccounts();
  // Fetch accounts in the Gaia cookies.
  GaiaCookieManagerService* cookie_manager_service =
      GaiaCookieManagerServiceFactory::GetForProfile(profile);
  std::vector<gaia::ListedAccount> cookie_accounts;
  bool gaia_accounts_stale = !cookie_manager_service->ListAccounts(
      &cookie_accounts, nullptr, "ProfileChooserView");
  UMA_HISTOGRAM_BOOLEAN("Profile.DiceUI.GaiaAccountsStale",
                        gaia_accounts_stale);
  // Fetch account information for each id and make sure that the first account
  // in the list matches the first account in the Gaia cookies (if available).
  AccountTrackerService* account_tracker_service =
      AccountTrackerServiceFactory::GetForProfile(profile);
  std::string gaia_default_account_id =
      cookie_accounts.empty() ? "" : cookie_accounts[0].id;
  std::vector<AccountInfo> accounts;
  for (const std::string& account_id : account_ids) {
    AccountInfo account_info =
        account_tracker_service->GetAccountInfo(account_id);
    if (account_id == gaia_default_account_id)
      accounts.insert(accounts.begin(), account_info);
    else
      accounts.push_back(account_info);
  }
  return accounts;
}

void EnableSync(Browser* browser,
                const AccountInfo& account,
                signin_metrics::AccessPoint access_point) {
  DCHECK(browser);
  DCHECK(!account.account_id.empty());
  DCHECK(!account.email.empty());
  DCHECK_NE(signin_metrics::AccessPoint::ACCESS_POINT_UNKNOWN, access_point);

  Profile* profile = browser->profile();
  DCHECK(AccountConsistencyModeManager::IsDiceEnabledForProfile(profile));
  if (SigninManagerFactory::GetForProfile(profile)->IsAuthenticated()) {
    DVLOG(1) << "There is already a primary account.";
    return;
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  bool needs_reauth_before_enable_sync =
      !token_service->RefreshTokenIsAvailable(account.account_id) ||
      token_service->RefreshTokenHasError(account.account_id);
  if (needs_reauth_before_enable_sync) {
    browser->signin_view_controller()->ShowDiceSigninTab(
        profiles::BUBBLE_VIEW_MODE_GAIA_SIGNIN, browser, access_point,
        account.email);
    return;
  }

  // DiceTurnSyncOnHelper is suicidal (it will delete itself once it finishes
  // enabling sync).
  new DiceTurnSyncOnHelper(
      profile, browser, access_point,
      signin_metrics::Reason::REASON_UNKNOWN_REASON, account.account_id,
      DiceTurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT);
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace signin_ui_util
