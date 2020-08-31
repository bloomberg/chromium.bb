// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/privacy_screen_controller.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/fake/fake_display_snapshot.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/test/action_logger_util.h"
#include "ui/display/manager/test/test_native_display_delegate.h"
#include "ui/display/types/display_constants.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@privacyscreen";
constexpr char kUser2Email[] = "user2@privacyscreen";
constexpr gfx::Size kDisplaySize{1024, 768};

class MockObserver : public PrivacyScreenController::Observer {
 public:
  MockObserver() {}
  ~MockObserver() override = default;

  MOCK_METHOD(void, OnPrivacyScreenSettingChanged, (bool enabled), (override));
};

class PrivacyScreenControllerTest : public NoSessionAshTestBase {
 public:
  PrivacyScreenControllerTest()
      : logger_(std::make_unique<display::test::ActionLogger>()) {}
  ~PrivacyScreenControllerTest() override = default;

  PrivacyScreenControllerTest(const PrivacyScreenControllerTest&) = delete;
  PrivacyScreenControllerTest& operator=(const PrivacyScreenControllerTest&) =
      delete;

  PrivacyScreenController* controller() {
    return Shell::Get()->privacy_screen_controller();
  }

  PrefService* user1_pref_service() const {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  void SetUp() override {
    NoSessionAshTestBase::SetUp();

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);

    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    native_display_delegate_ =
        new display::test::TestNativeDisplayDelegate(logger_.get());
    display_manager()->configurator()->SetDelegateForTesting(
        std::unique_ptr<display::NativeDisplayDelegate>(
            native_display_delegate_));
    display_change_observer_ =
        std::make_unique<display::DisplayChangeObserver>(display_manager());
    test_api_ = std::make_unique<display::DisplayConfigurator::TestApi>(
        display_manager()->configurator());

    controller()->AddObserver(observer());
  }

  struct TestSnapshotParams {
    int64_t id;
    bool is_internal_display;
    bool supports_privacy_screen;
  };

  void TearDown() override {
    // DisplayChangeObserver access DeviceDataManager in its destructor, so
    // destroy it first.
    display_change_observer_ = nullptr;
    controller()->RemoveObserver(observer());
    AshTestBase::TearDown();
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  // Builds displays snapshots into |owned_snapshots_| and update the display
  // configurator and display manager with it.
  void BuildAndUpdateDisplaySnapshots(
      const std::vector<TestSnapshotParams>& snapshot_params) {
    owned_snapshots_.clear();
    std::vector<display::DisplaySnapshot*> outputs;

    for (const auto& param : snapshot_params) {
      owned_snapshots_.emplace_back(
          display::FakeDisplaySnapshot::Builder()
              .SetId(param.id)
              .SetNativeMode(kDisplaySize)
              .SetCurrentMode(kDisplaySize)
              .SetType(param.is_internal_display
                           ? display::DISPLAY_CONNECTION_TYPE_INTERNAL
                           : display::DISPLAY_CONNECTION_TYPE_HDMI)
              .SetPrivacyScreen(param.supports_privacy_screen
                                    ? display::kDisabled
                                    : display::kNotSupported)
              .Build());
      outputs.push_back(owned_snapshots_.back().get());
    }

    native_display_delegate_->set_outputs(outputs);
    display_manager()->configurator()->OnConfigurationChanged();
    display_manager()->configurator()->ForceInitialConfigure();
    EXPECT_TRUE(test_api_->TriggerConfigureTimeout());
    display_change_observer_->OnDisplayModeChanged(outputs);
  }

  MockObserver* observer() { return &observer_; }

 private:
  std::unique_ptr<display::test::ActionLogger> logger_;
  display::test::TestNativeDisplayDelegate*
      native_display_delegate_;  // Not owned.
  std::unique_ptr<display::DisplayChangeObserver> display_change_observer_;
  std::unique_ptr<display::DisplayConfigurator::TestApi> test_api_;
  std::vector<std::unique_ptr<display::DisplaySnapshot>> owned_snapshots_;
  ::testing::NiceMock<MockObserver> observer_;
};

// Test that user prefs do not get mixed up between user changes on a device
// with a single supporting display.
TEST_F(PrivacyScreenControllerTest, TestEnableAndDisable) {
  // Create a single internal display that supports privacy screen.
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  // Enable for user 1, and switch to user 2. User 2 should have it disabled.
  controller()->SetEnabled(true);
  // Switching accounts shouldn't trigger observers.
  ::testing::Mock::VerifyAndClear(observer());
  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged).Times(0);
  EXPECT_TRUE(controller()->GetEnabled());
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller()->GetEnabled());

  // Switch back to user 1, expect it to be enabled.
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller()->GetEnabled());
}

// Tests that updates of the Privacy Screen user prefs from outside the
// PrivacyScreenController (such as Settings UI) are observed and applied.
TEST_F(PrivacyScreenControllerTest, TestOutsidePrefsUpdates) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/true,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(true));
  EXPECT_FALSE(controller()->GetEnabled());
  user1_pref_service()->SetBoolean(prefs::kDisplayPrivacyScreenEnabled, true);
  EXPECT_TRUE(controller()->GetEnabled());

  EXPECT_CALL(*observer(), OnPrivacyScreenSettingChanged(false));
  user1_pref_service()->SetBoolean(prefs::kDisplayPrivacyScreenEnabled, false);
  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest, NotSupportedOnSingleInternalDisplay) {
  BuildAndUpdateDisplaySnapshots({{
      /*id=*/123u,
      /*is_internal_display=*/true,
      /*supports_privacy_screen=*/false,
  }});
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  ASSERT_FALSE(controller()->IsSupported());

  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest,
       SupportedOnInternalDisplayWithMultipleExternalDisplays) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/1234u,
                                      /*is_internal_display=*/true,
                                      /*supports_privacy_screen=*/true,
                                  },
                                  {
                                      /*id=*/2341u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/3412u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/4123u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  }});
  EXPECT_EQ(4u, display_manager()->GetNumDisplays());
  ASSERT_TRUE(controller()->IsSupported());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest,
       NotSupportedOnInternalDisplayWithMultipleExternalDisplays) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/1234u,
                                      /*is_internal_display=*/true,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/2341u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/3412u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/4123u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  }});
  EXPECT_EQ(4u, display_manager()->GetNumDisplays());
  ASSERT_FALSE(controller()->IsSupported());

  EXPECT_FALSE(controller()->GetEnabled());
}

TEST_F(PrivacyScreenControllerTest,
       NotSupportedOnMultipleSupportingExternalDisplays) {
  BuildAndUpdateDisplaySnapshots({{
                                      /*id=*/1234u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/false,
                                  },
                                  {
                                      /*id=*/2341u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/true,
                                  },
                                  {
                                      /*id=*/3412u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/true,
                                  },
                                  {
                                      /*id=*/4123u,
                                      /*is_internal_display=*/false,
                                      /*supports_privacy_screen=*/true,
                                  }});
  EXPECT_EQ(4u, display_manager()->GetNumDisplays());
  ASSERT_FALSE(controller()->IsSupported());

  EXPECT_FALSE(controller()->GetEnabled());
}

}  // namespace

}  // namespace ash
