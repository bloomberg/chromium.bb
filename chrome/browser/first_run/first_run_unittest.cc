// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/test/scoped_path_override.h"
#include "base/values.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/installer/util/master_preferences.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace first_run {

class FirstRunTest : public testing::Test {
 protected:
  FirstRunTest() : user_data_dir_override_(chrome::DIR_USER_DATA) {}
  ~FirstRunTest() override {}

 private:
  base::ScopedPathOverride user_data_dir_override_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunTest);
};

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_VariationsSeed) {
  installer::MasterPreferences install_prefs("{ \"variations_seed\":\"xyz\" }");
  EXPECT_EQ(1U, install_prefs.master_dictionary().size());

  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_EQ("xyz", out_prefs.variations_seed);
  // Variations prefs should have been extracted (removed) from the dictionary.
  EXPECT_TRUE(install_prefs.master_dictionary().empty());
}

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_NoVariationsSeed) {
  installer::MasterPreferences install_prefs("{ }");
  EXPECT_TRUE(install_prefs.master_dictionary().empty());

  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_TRUE(out_prefs.variations_seed.empty());
  EXPECT_TRUE(out_prefs.variations_seed_signature.empty());
}

TEST_F(FirstRunTest, SetupMasterPrefsFromInstallPrefs_VariationsSeedSignature) {
  installer::MasterPreferences install_prefs(
      "{ \"variations_seed\":\"xyz\", \"variations_seed_signature\":\"abc\" }");
  EXPECT_EQ(2U, install_prefs.master_dictionary().size());

  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_EQ("xyz", out_prefs.variations_seed);
  EXPECT_EQ("abc", out_prefs.variations_seed_signature);
  // Variations prefs should have been extracted (removed) from the dictionary.
  EXPECT_TRUE(install_prefs.master_dictionary().empty());
}

TEST_F(FirstRunTest,
       SetupMasterPrefsFromInstallPrefs_WelcomePageOnOSUpgradeMissing) {
  installer::MasterPreferences install_prefs("{\"distribution\":{}}");
  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_TRUE(out_prefs.welcome_page_on_os_upgrade_enabled);
}

TEST_F(FirstRunTest,
       SetupMasterPrefsFromInstallPrefs_WelcomePageOnOSUpgradeEnabled) {
  installer::MasterPreferences install_prefs(
      "{\"distribution\":{\"welcome_page_on_os_upgrade_enabled\": true}}");
  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_TRUE(out_prefs.welcome_page_on_os_upgrade_enabled);
}

TEST_F(FirstRunTest,
       SetupMasterPrefsFromInstallPrefs_WelcomePageOnOSUpgradeDisabled) {
  installer::MasterPreferences install_prefs(
      "{\"distribution\":{\"welcome_page_on_os_upgrade_enabled\": false}}");
  MasterPrefs out_prefs;
  internal::SetupMasterPrefsFromInstallPrefs(install_prefs, &out_prefs);
  EXPECT_FALSE(out_prefs.welcome_page_on_os_upgrade_enabled);
}

// No switches and no sentinel present. This is the standard case for first run.
TEST_F(FirstRunTest, DetermineFirstRunState_FirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// Force switch is present, overriding both sentinel and suppress switch.
TEST_F(FirstRunTest, DetermineFirstRunState_ForceSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(true, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, true);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);

  result = internal::DetermineFirstRunState(false, true, false);
  EXPECT_EQ(internal::FIRST_RUN_TRUE, result);
}

// No switches, but sentinel present. This is not a first run.
TEST_F(FirstRunTest, DetermineFirstRunState_NotFirstRun) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(true, false, false);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

// Suppress switch is present, overriding sentinel state.
TEST_F(FirstRunTest, DetermineFirstRunState_SuppressSwitch) {
  internal::FirstRunState result =
      internal::DetermineFirstRunState(false, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);

  result = internal::DetermineFirstRunState(true, false, true);
  EXPECT_EQ(internal::FIRST_RUN_FALSE, result);
}

}  // namespace first_run
