// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_elf_init_win.h"

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeBlacklistTrialTest : public testing::Test {
 protected:
  ChromeBlacklistTrialTest() :
      blacklist_registry_key_(HKEY_CURRENT_USER,
                              blacklist::kRegistryBeaconPath,
                              KEY_QUERY_VALUE | KEY_SET_VALUE) {}

  virtual void SetUp() OVERRIDE {
    testing::Test::SetUp();

    registry_util::RegistryOverrideManager override_manager;
    override_manager.OverrideRegistry(HKEY_CURRENT_USER,
                                      L"browser_blacklist_test");


  }

  DWORD GetBlacklistState() {
    DWORD blacklist_state = blacklist::BLACKLIST_STATE_MAX;
    blacklist_registry_key_.ReadValueDW(blacklist::kBeaconState,
                                        &blacklist_state);

    return blacklist_state;
  }

  base::string16 GetBlacklistVersion() {
    base::string16 blacklist_version;
    blacklist_registry_key_.ReadValue(blacklist::kBeaconVersion,
                                      &blacklist_version);

    return blacklist_version;
  }

  base::win::RegKey blacklist_registry_key_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeBlacklistTrialTest);
};


// Ensure that the default trial deletes any existing blacklist beacons.
TEST_F(ChromeBlacklistTrialTest, DefaultRun) {
  // Set some dummy values as beacons.
  blacklist_registry_key_.WriteValue(blacklist::kBeaconState,
                                     blacklist::BLACKLIST_ENABLED);
  blacklist_registry_key_.WriteValue(blacklist::kBeaconVersion, L"Data");

  // This setup code should result in the default group, which should remove
  // all the beacon values.
  InitializeChromeElf();

  // Ensure that invalid values are returned to indicate that the
  // beacon values are gone.
  ASSERT_EQ(blacklist::BLACKLIST_STATE_MAX, GetBlacklistState());
  ASSERT_EQ(base::string16(), GetBlacklistVersion());
}

TEST_F(ChromeBlacklistTrialTest, VerifyFirstRun) {
  BrowserBlacklistBeaconSetup();

  // Verify the state is properly set after the first run.
  ASSERT_EQ(blacklist::BLACKLIST_ENABLED, GetBlacklistState());

  chrome::VersionInfo version_info;
  base::string16 version(base::UTF8ToUTF16(version_info.Version()));
  ASSERT_EQ(version, GetBlacklistVersion());
}

TEST_F(ChromeBlacklistTrialTest, SetupFailed) {
  // Set the registry to indicate that the blacklist setup is running,
  // which means it failed to run correctly last time.
  blacklist_registry_key_.WriteValue(blacklist::kBeaconState,
                                     blacklist::BLACKLIST_SETUP_RUNNING);

   BrowserBlacklistBeaconSetup();

  // Since the blacklist setup failed, it should now be disabled.
  ASSERT_EQ(blacklist::BLACKLIST_DISABLED, GetBlacklistState());
}

TEST_F(ChromeBlacklistTrialTest, ThunkSetupFailed) {
  // Set the registry to indicate that the blacklist thunk setup is running,
  // which means it failed to run correctly last time.
  blacklist_registry_key_.WriteValue(blacklist::kBeaconState,
                                     blacklist::BLACKLIST_THUNK_SETUP);

  BrowserBlacklistBeaconSetup();

  // Since the blacklist thunk setup failed, it should now be disabled.
  ASSERT_EQ(blacklist::BLACKLIST_DISABLED, GetBlacklistState());
}

TEST_F(ChromeBlacklistTrialTest, InterceptionFailed) {
  // Set the registry to indicate that an interception is running,
  // which means it failed to run correctly last time.
  blacklist_registry_key_.WriteValue(blacklist::kBeaconState,
                                     blacklist::BLACKLIST_INTERCEPTING);

  BrowserBlacklistBeaconSetup();

  // Since an interception failed, the blacklist should now be disabled.
  ASSERT_EQ(blacklist::BLACKLIST_DISABLED, GetBlacklistState());
}

TEST_F(ChromeBlacklistTrialTest, VersionChanged) {
  // Mark the blacklist as disabled for an older version, so it should
  // get enabled for this new version.
  blacklist_registry_key_.WriteValue(blacklist::kBeaconVersion,
                                     L"old_version");
  blacklist_registry_key_.WriteValue(blacklist::kBeaconState,
                                     blacklist::BLACKLIST_DISABLED);

  BrowserBlacklistBeaconSetup();

  // The beacon should now be marked as enabled for the current version.
  ASSERT_EQ(blacklist::BLACKLIST_ENABLED, GetBlacklistState());

  chrome::VersionInfo version_info;
  base::string16 expected_version(base::UTF8ToUTF16(version_info.Version()));
  ASSERT_EQ(expected_version, GetBlacklistVersion());
}
