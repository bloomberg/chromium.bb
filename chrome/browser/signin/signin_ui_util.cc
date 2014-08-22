// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/prefs/pref_service.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_global_error.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_global_error.h"
#include "chrome/browser/sync/sync_global_error_factory.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"

namespace {
// Maximum width of a username - we trim emails that are wider than this so
// the wrench menu doesn't get ridiculously wide.
const int kUsernameMaxWidth = 200;
}  // namespace

namespace signin_ui_util {

GlobalError* GetSignedInServiceError(Profile* profile) {
  std::vector<GlobalError*> errors = GetSignedInServiceErrors(profile);
  if (errors.empty())
    return NULL;
  return errors[0];
}

std::vector<GlobalError*> GetSignedInServiceErrors(Profile* profile) {
  std::vector<GlobalError*> errors;
  // Chrome OS doesn't use SigninGlobalError or SyncGlobalError. Other platforms
  // may use these services to show auth and sync errors in the toolbar menu.
#if !defined(OS_CHROMEOS)
  // Auth errors have the highest priority - after that, individual service
  // errors.
  SigninGlobalError* signin_error =
      SigninGlobalErrorFactory::GetForProfile(profile);
  if (signin_error && signin_error->HasError())
    errors.push_back(signin_error);

  // No auth error - now try other services. Currently the list is just hard-
  // coded but in the future if we add more we can create some kind of
  // registration framework.
  if (profile->IsSyncAccessible()) {
    SyncGlobalError* error = SyncGlobalErrorFactory::GetForProfile(profile);
    if (error && error->HasMenuItem())
      errors.push_back(error);
  }
#endif

  return errors;
}

base::string16 GetSigninMenuLabel(Profile* profile) {
  GlobalError* error = signin_ui_util::GetSignedInServiceError(profile);
  if (error)
    return error->MenuItemLabel();

  // No errors, so just display the signed in user, if any.
  ProfileSyncService* service = profile->IsSyncAccessible() ?
      ProfileSyncServiceFactory::GetForProfile(profile) : NULL;

  // Even if the user is signed in, don't display the "signed in as..."
  // label if we're still setting up sync.
  if (!service || !service->FirstSetupInProgress()) {
    std::string username;
    SigninManagerBase* signin_manager =
        SigninManagerFactory::GetForProfileIfExists(profile);
    if (signin_manager)
      username = signin_manager->GetAuthenticatedUsername();
    if (!username.empty() && !signin_manager->AuthInProgress()) {
      const base::string16 elided = gfx::ElideText(base::UTF8ToUTF16(username),
          gfx::FontList(), kUsernameMaxWidth, gfx::ELIDE_EMAIL);
      return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_SYNCED_LABEL, elided);
    }
  }
  return l10n_util::GetStringFUTF16(IDS_SYNC_MENU_PRE_SYNCED_LABEL,
      l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME));
}

// Given an authentication state this helper function returns various labels
// that can be used to display information about the state.
void GetStatusLabelsForAuthError(Profile* profile,
                                 const SigninManagerBase& signin_manager,
                                 base::string16* status_label,
                                 base::string16* link_label) {
  base::string16 username =
      base::UTF8ToUTF16(signin_manager.GetAuthenticatedUsername());
  base::string16 product_name = l10n_util::GetStringUTF16(IDS_PRODUCT_NAME);
  if (link_label)
    link_label->assign(l10n_util::GetStringUTF16(IDS_SYNC_RELOGIN_LINK_LABEL));

  const GoogleServiceAuthError::State state =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->
          signin_error_controller()->auth_error().state();
  switch (state) {
    case GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS:
    case GoogleServiceAuthError::SERVICE_ERROR:
    case GoogleServiceAuthError::ACCOUNT_DELETED:
    case GoogleServiceAuthError::ACCOUNT_DISABLED:
      // If the user name is empty then the first login failed, otherwise the
      // credentials are out-of-date.
      if (username.empty()) {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_INVALID_USER_CREDENTIALS));
        }
      } else {
        if (status_label) {
          status_label->assign(
              l10n_util::GetStringUTF16(IDS_SYNC_LOGIN_INFO_OUT_OF_DATE));
        }
      }
      break;
    case GoogleServiceAuthError::SERVICE_UNAVAILABLE:
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringUTF16(IDS_SYNC_SERVICE_UNAVAILABLE));
      }
      if (link_label)
        link_label->clear();
      break;
    case GoogleServiceAuthError::CONNECTION_FAILED:
      if (status_label) {
        status_label->assign(
            l10n_util::GetStringFUTF16(IDS_SYNC_SERVER_IS_UNREACHABLE,
                                       product_name));
      }
      // Note that there is little the user can do if the server is not
      // reachable. Since attempting to re-connect is done automatically by
      // the Syncer, we do not show the (re)login link.
      if (link_label)
        link_label->clear();
      break;
    default:
      if (status_label) {
        status_label->assign(l10n_util::GetStringUTF16(
            IDS_SYNC_ERROR_SIGNING_IN));
      }
      break;
  }
}

void InitializePrefsForProfile(Profile* profile) {
  // Suppresses the upgrade tutorial for a new profile.
  if (profile->IsNewProfile() && switches::IsNewAvatarMenu()) {
    profile->GetPrefs()->SetInteger(
        prefs::kProfileAvatarTutorialShown, kUpgradeWelcomeTutorialShowMax + 1);
  }
}

void ShowSigninErrorLearnMorePage(Profile* profile) {
  static const char kSigninErrorLearnMoreUrl[] =
      "https://support.google.com/chrome/answer/1181420?";
  chrome::NavigateParams params(
      profile, GURL(kSigninErrorLearnMoreUrl), content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

}  // namespace signin_ui_util
