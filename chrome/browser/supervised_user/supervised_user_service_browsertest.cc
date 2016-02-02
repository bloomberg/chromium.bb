// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/browser/supervised_user/supervised_user_service.h"
#include "chrome/browser/supervised_user/supervised_user_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service.h"
#include "chrome/browser/supervised_user/supervised_user_settings_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/google_service_auth_error.h"

namespace {

void TestAuthErrorCallback(const GoogleServiceAuthError& error) {}

class SupervisedUserServiceTestSupervised : public InProcessBrowserTest {
 public:
  // content::BrowserTestBase:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII(switches::kSupervisedUserId, "asdf");
  }
};

}  // namespace

typedef InProcessBrowserTest SupervisedUserServiceTest;

// Crashes on Mac.
// http://crbug.com/339501
#if defined(OS_MACOSX)
#define MAYBE_ClearOmitOnRegistration DISABLED_ClearOmitOnRegistration
#else
#define MAYBE_ClearOmitOnRegistration ClearOmitOnRegistration
#endif
// Ensure that a profile that has completed registration is included in the
// list shown in the avatar menu.
IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest,
                       MAYBE_ClearOmitOnRegistration) {
  // Artificially mark the profile as omitted.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  Profile* profile = browser()->profile();
  size_t index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  cache.SetIsOmittedProfileAtIndex(index, true);
  ASSERT_TRUE(cache.IsOmittedProfileAtIndex(index));

  SupervisedUserService* supervised_user_service =
      SupervisedUserServiceFactory::GetForProfile(profile);

  // A registration error does not clear the flag (the profile should be deleted
  // anyway).
  supervised_user_service->OnSupervisedUserRegistered(
      base::Bind(&TestAuthErrorCallback),
      profile,
      GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED),
      std::string());
  ASSERT_TRUE(cache.IsOmittedProfileAtIndex(index));

  // Successfully completing registration clears the flag.
  supervised_user_service->OnSupervisedUserRegistered(
      base::Bind(&TestAuthErrorCallback),
      profile,
      GoogleServiceAuthError(GoogleServiceAuthError::NONE),
      std::string("abcdef"));
  EXPECT_FALSE(cache.IsOmittedProfileAtIndex(index));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceGoogleSafeSearch));
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceYouTubeSafetyMode));
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(
      prefs->IsUserModifiablePreference(prefs::kForceYouTubeSafetyMode));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTest, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->IsUserModifiablePreference(prefs::kProfileName));

  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceGoogleSafeSearch));
  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceYouTubeSafetyMode));
  EXPECT_FALSE(
      prefs->IsUserModifiablePreference(prefs::kForceGoogleSafeSearch));
  EXPECT_FALSE(
      prefs->IsUserModifiablePreference(prefs::kForceYouTubeSafetyMode));
}

IN_PROC_BROWSER_TEST_F(SupervisedUserServiceTestSupervised, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();

  SupervisedUserSettingsService* settings =
      SupervisedUserSettingsServiceFactory::GetForProfile(profile);

  std::string name = "Supervised User Test Name";
  settings->SetLocalSetting(
      supervised_users::kUserName,
      scoped_ptr<base::Value>(new base::StringValue(name)));
  EXPECT_FALSE(prefs->IsUserModifiablePreference(prefs::kProfileName));
  EXPECT_EQ(name, prefs->GetString(prefs::kProfileName));
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Change the name once more.
  std::string new_name = "New Supervised User Test Name";
  settings->SetLocalSetting(
      supervised_users::kUserName,
      scoped_ptr<base::Value>(new base::StringValue(new_name)));
  EXPECT_EQ(new_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(new_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Remove the setting.
  settings->SetLocalSetting(supervised_users::kUserName,
                            scoped_ptr<base::Value>());
  EXPECT_EQ(original_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            base::UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
}
