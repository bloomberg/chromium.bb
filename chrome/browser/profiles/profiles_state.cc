// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profiles_state.h"

#include "base/files/file_path.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/gaia_info_update_service.h"
#include "chrome/browser/profiles/gaia_info_update_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/common/profile_management_switches.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#endif

namespace profiles {

bool IsMultipleProfilesEnabled() {
#if defined(OS_ANDROID)
  return false;
#endif
  return true;
}

base::FilePath GetDefaultProfileDir(const base::FilePath& user_data_dir) {
  base::FilePath default_profile_dir(user_data_dir);
  default_profile_dir =
      default_profile_dir.AppendASCII(chrome::kInitialProfile);
  return default_profile_dir;
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kProfileLastUsed, std::string());
  registry->RegisterIntegerPref(prefs::kProfilesNumCreated, 1);
  registry->RegisterListPref(prefs::kProfilesLastActive);
}

base::string16 GetAvatarNameForProfile(const base::FilePath& profile_path) {
  base::string16 display_name;

  if (profile_path == ProfileManager::GetGuestProfilePath()) {
    display_name = l10n_util::GetStringUTF16(IDS_GUEST_PROFILE_NAME);
  } else {
    ProfileInfoCache& cache =
        g_browser_process->profile_manager()->GetProfileInfoCache();
    size_t index = cache.GetIndexOfProfileWithPath(profile_path);

    if (index == std::string::npos)
      return l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);

    // Using the --new-profile-management flag, there's a couple of rules
    // about what the avatar button displays. If there's a single, local
    // profile, with a default name (i.e. of the form Person %d), it should
    // display IDS_SINGLE_PROFILE_DISPLAY_NAME. If this is a signed in profile,
    // or the user has edited the profile name, or there are multiple profiles,
    // it will return the actual name  of the profile.
    base::string16 profile_name = cache.GetNameOfProfileAtIndex(index);
    bool has_default_name = cache.ProfileIsUsingDefaultNameAtIndex(index);

    if (cache.GetNumberOfProfiles() == 1 && has_default_name &&
        cache.GetUserNameOfProfileAtIndex(index).empty()) {
      display_name = l10n_util::GetStringUTF16(IDS_SINGLE_PROFILE_DISPLAY_NAME);
    } else {
      display_name = profile_name;
    }
  }
  return display_name;
}

void UpdateProfileName(Profile* profile,
                       const base::string16& new_profile_name) {
  PrefService* pref_service = profile->GetPrefs();
  // Updating the profile preference will cause the cache to be updated for
  // this preference.
  pref_service->SetString(prefs::kProfileName,
                          base::UTF16ToUTF8(new_profile_name));
}

std::vector<std::string> GetSecondaryAccountsForProfile(
    Profile* profile,
    const std::string& primary_account) {
  std::vector<std::string> accounts =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile)->GetAccounts();

  // The vector returned by ProfileOAuth2TokenService::GetAccounts() contains
  // the primary account too, so we need to remove it from the list.
  std::vector<std::string>::iterator primary_index =
      std::find_if(accounts.begin(), accounts.end(),
                   std::bind1st(std::equal_to<std::string>(), primary_account));
  DCHECK(primary_index != accounts.end());
  accounts.erase(primary_index);

  return accounts;
}

bool IsRegularOrGuestSession(Browser* browser) {
  Profile* profile = browser->profile();
  return profile->IsGuestSession() || !profile->IsOffTheRecord();
}

void UpdateGaiaProfilePhotoIfNeeded(Profile* profile) {
  // If the --google-profile-info flag isn't used, then the
  // GAIAInfoUpdateService isn't initialized, and we can't download the picture.
  if (!switches::IsGoogleProfileInfo())
    return;

  DCHECK(profile);
  GAIAInfoUpdateServiceFactory::GetInstance()->GetForProfile(profile)->Update();
}

SigninErrorController* GetSigninErrorController(Profile* profile) {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  return token_service ? token_service->signin_error_controller() : NULL;
}

}  // namespace profiles
