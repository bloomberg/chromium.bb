// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/beacons.h"

#include <memory>
#include <tuple>

#include "base/memory/ptr_util.h"
#include "base/test/test_reg_util_win.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/win/registry.h"
#include "base/win/win_util.h"
#include "chrome/install_static/install_details.h"
#include "chrome/install_static/install_modes.h"
#include "chrome/install_static/test/scoped_install_details.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/install_util.h"
#include "chrome/installer/util/test_app_registration_data.h"
#include "chrome/installer/util/util_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Bool;
using ::testing::Combine;
using ::testing::Values;
using BeaconType = installer_util::Beacon::BeaconType;
using BeaconScope = installer_util::Beacon::BeaconScope;

namespace installer_util {

// A test fixture that exercises a beacon.
class BeaconTest : public ::testing::TestWithParam<
                       ::testing::tuple<BeaconType, BeaconScope, bool>> {
 protected:
  static const base::char16 kBeaconName[];

  BeaconTest()
      : beacon_type_(::testing::get<0>(GetParam())),
        beacon_scope_(::testing::get<1>(GetParam())),
        system_install_(::testing::get<2>(GetParam())),
        beacon_(kBeaconName,
                beacon_type_,
                beacon_scope_,
                system_install_,
                app_registration_data_) {}

  void SetUp() override {
    // Override the registry so that tests can freely push state to it.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));
  }

  TestAppRegistrationData app_registration_data_;
  BeaconType beacon_type_;
  BeaconScope beacon_scope_;
  bool system_install_;
  Beacon beacon_;

 private:
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// static
const base::char16 BeaconTest::kBeaconName[] = L"TestBeacon";

// Nothing in the regsitry, so the beacon should not exist.
TEST_P(BeaconTest, GetNonExistant) {
  ASSERT_TRUE(beacon_.Get().is_null());
}

// Updating and then getting the beacon should return a value, and that it is
// within range.
TEST_P(BeaconTest, UpdateAndGet) {
  base::Time before(base::Time::Now());
  beacon_.Update();
  base::Time after(base::Time::Now());
  base::Time beacon_time(beacon_.Get());
  ASSERT_FALSE(beacon_time.is_null());
  ASSERT_LE(before, beacon_time);
  ASSERT_GE(after, beacon_time);
}

// Tests that updating a first beacon only updates it the first time, but doing
// so for a last beacon always updates.
TEST_P(BeaconTest, UpdateTwice) {
  beacon_.Update();
  base::Time beacon_time(beacon_.Get());

  base::PlatformThread::Sleep(TestTimeouts::tiny_timeout());

  beacon_.Update();
  if (beacon_type_ == BeaconType::FIRST) {
    ASSERT_EQ(beacon_time, beacon_.Get());
  } else {
    ASSERT_NE(beacon_time, beacon_.Get());
  }
}

// Tests that the beacon is written into the proper location in the registry.
TEST_P(BeaconTest, Location) {
  beacon_.Update();
  HKEY right_root = system_install_ ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
  HKEY wrong_root = system_install_ ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
  base::string16 right_key;
  base::string16 wrong_key;
  base::string16 value_name;

  if (beacon_scope_ == BeaconScope::PER_INSTALL || !system_install_) {
    value_name = kBeaconName;
    right_key = app_registration_data_.GetStateKey();
    wrong_key = app_registration_data_.GetStateMediumKey();
  } else {
    ASSERT_TRUE(base::win::GetUserSidString(&value_name));
    right_key =
        app_registration_data_.GetStateMediumKey() + L"\\" + kBeaconName;
    wrong_key = app_registration_data_.GetStateKey();
  }

  // Keys should not exist in the wrong root or in the right root but wrong key.
  ASSERT_FALSE(base::win::RegKey(wrong_root, right_key.c_str(),
                                 KEY_READ).Valid()) << right_key;
  ASSERT_FALSE(base::win::RegKey(wrong_root, wrong_key.c_str(),
                                 KEY_READ).Valid()) << wrong_key;
  ASSERT_FALSE(base::win::RegKey(right_root, wrong_key.c_str(),
                                 KEY_READ).Valid()) << wrong_key;
  // The right key should exist.
  base::win::RegKey key(right_root, right_key.c_str(), KEY_READ);
  ASSERT_TRUE(key.Valid()) << right_key;
  // And should have the value.
  ASSERT_TRUE(key.HasValue(value_name.c_str())) << value_name;
}

// Run the tests for all combinations of beacon type, scope, and install level.
INSTANTIATE_TEST_CASE_P(BeaconTest,
                        BeaconTest,
                        Combine(Values(BeaconType::FIRST, BeaconType::LAST),
                                Values(BeaconScope::PER_USER,
                                       BeaconScope::PER_INSTALL),
                                Bool()));

class DefaultBrowserBeaconTest
    : public ::testing::TestWithParam<
          std::tuple<install_static::InstallConstantIndex, const char*>> {
 protected:
  using Super = ::testing::TestWithParam<
      std::tuple<install_static::InstallConstantIndex, const char*>>;

  void SetUp() override {
    Super::SetUp();

    install_static::InstallConstantIndex mode_index;
    const char* level;
    std::tie(mode_index, level) = GetParam();

    system_install_ = (std::string(level) != "user");

    // Configure InstallDetails for the test.
    scoped_install_details_ =
        base::MakeUnique<install_static::ScopedInstallDetails>(system_install_,
                                                               mode_index);
    // Override the registry so that tests can freely push state to it.
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_CURRENT_USER));
    ASSERT_NO_FATAL_FAILURE(
        registry_override_manager_.OverrideRegistry(HKEY_LOCAL_MACHINE));

    // Ensure that IsPerUserInstall returns the proper value.
    ASSERT_EQ(!system_install_, InstallUtil::IsPerUserInstall());

    distribution_ = BrowserDistribution::GetDistribution();
  }

  bool system_install_ = false;
  BrowserDistribution* distribution_ = nullptr;

 private:
  std::unique_ptr<install_static::ScopedInstallDetails> scoped_install_details_;
  registry_util::RegistryOverrideManager registry_override_manager_;
};

// Tests that the default browser beacons work as expected.
TEST_P(DefaultBrowserBeaconTest, All) {
  std::unique_ptr<Beacon> last_was_default(MakeLastWasDefaultBeacon(
      system_install_, distribution_->GetAppRegistrationData()));
  std::unique_ptr<Beacon> first_not_default(MakeFirstNotDefaultBeacon(
      system_install_, distribution_->GetAppRegistrationData()));

  ASSERT_TRUE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // Chrome is not default.
  UpdateDefaultBrowserBeaconWithState(distribution_, ShellUtil::NOT_DEFAULT);
  ASSERT_TRUE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());

  // Then it is.
  UpdateDefaultBrowserBeaconWithState(distribution_, ShellUtil::IS_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // It still is.
  UpdateDefaultBrowserBeaconWithState(distribution_, ShellUtil::IS_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_TRUE(first_not_default->Get().is_null());

  // Now it's not again.
  UpdateDefaultBrowserBeaconWithState(distribution_, ShellUtil::NOT_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());

  // And it still isn't.
  UpdateDefaultBrowserBeaconWithState(distribution_, ShellUtil::NOT_DEFAULT);
  ASSERT_FALSE(last_was_default->Get().is_null());
  ASSERT_FALSE(first_not_default->Get().is_null());
}

#if defined(GOOGLE_CHROME_BUILD)
// Stable supports user and system levels.
INSTANTIATE_TEST_CASE_P(
    Stable,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::STABLE_INDEX),
                     testing::Values("user", "system")));
// Canary is only at user level.
INSTANTIATE_TEST_CASE_P(
    Canary,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::CANARY_INDEX),
                     testing::Values("user")));
#else   // GOOGLE_CHROME_BUILD
// Chromium supports user and system levels.
INSTANTIATE_TEST_CASE_P(
    Chromium,
    DefaultBrowserBeaconTest,
    testing::Combine(testing::Values(install_static::CHROMIUM_INDEX),
                     testing::Values("user", "system")));
#endif  // GOOGLE_CHROME_BUILD

}  // namespace installer_util
