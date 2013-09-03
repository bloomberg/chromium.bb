// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode/managed_user_service.h"
#include "chrome/browser/managed_mode/managed_user_service_factory.h"
#include "chrome/browser/policy/managed_mode_policy_provider.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/test_utils.h"
#include "policy/policy_constants.h"

typedef InProcessBrowserTest ManagedUserServiceTest;

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, LocalPolicies) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kForceSafeSearch));

  ManagedUserService* managed_user_service =
       ManagedUserServiceFactory::GetForProfile(profile);
  managed_user_service->InitForTesting();
  content::RunAllPendingInMessageLoop();

  EXPECT_TRUE(prefs->GetBoolean(prefs::kForceSafeSearch));
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kForceSafeSearch));
}

IN_PROC_BROWSER_TEST_F(ManagedUserServiceTest, ProfileName) {
  Profile* profile = browser()->profile();
  PrefService* prefs = profile->GetPrefs();
  EXPECT_FALSE(prefs->IsManagedPreference(prefs::kProfileName));

  std::string original_name = prefs->GetString(prefs::kProfileName);
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  const ProfileInfoCache& cache = profile_manager->GetProfileInfoCache();
  size_t profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Change the profile to a managed user.
  ManagedUserService* managed_user_service =
      ManagedUserServiceFactory::GetForProfile(profile);
  managed_user_service->InitForTesting();
  policy::ProfilePolicyConnector* connector =
      policy::ProfilePolicyConnectorFactory::GetForProfile(profile);
  policy::ManagedModePolicyProvider* provider =
      connector->managed_mode_policy_provider();

  std::string name = "Managed User Test Name";
  provider->SetLocalPolicyForTesting(
      policy::key::kUserDisplayName,
      scoped_ptr<base::Value>(new base::StringValue(name)));
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(prefs->IsManagedPreference(prefs::kProfileName));
  EXPECT_EQ(name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(name, UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Change the name once more.
  std::string new_name = "New Managed User Test Name";
  provider->SetLocalPolicyForTesting(
      policy::key::kUserDisplayName,
      scoped_ptr<base::Value>(new base::StringValue(new_name)));
  content::RunAllPendingInMessageLoop();;
  EXPECT_EQ(new_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(new_name,
            UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));

  // Remove the policy.
  provider->SetLocalPolicyForTesting(policy::key::kUserDisplayName,
                                     scoped_ptr<base::Value>());
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(original_name, prefs->GetString(prefs::kProfileName));
  profile_index = cache.GetIndexOfProfileWithPath(profile->GetPath());
  EXPECT_EQ(original_name,
            UTF16ToUTF8(cache.GetNameOfProfileAtIndex(profile_index)));
}
