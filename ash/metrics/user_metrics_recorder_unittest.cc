// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/metrics/user_metrics_recorder.h"

#include "ash/shell.h"
#include "ash/system/user/login_status.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/user_metrics_recorder_test_api.h"
#include "base/memory/scoped_ptr.h"
#include "base/test/histogram_tester.h"
#include "ui/aura/window.h"

namespace ash {
namespace {

const char kAsh_NumberOfVisibleWindowsInPrimaryDisplay[] =
    "Ash.NumberOfVisibleWindowsInPrimaryDisplay";

const char kAsh_ActiveWindowShowTypeOverTime[] =
    "Ash.ActiveWindowShowTypeOverTime";

}  // namespace

// Test fixture for the UserMetricsRecorder class.
class UserMetricsRecorderTest : public test::AshTestBase {
 public:
  UserMetricsRecorderTest();
  ~UserMetricsRecorderTest() override;

  // test::AshTestBase:
  void SetUp() override;
  void TearDown() override;

  // Sets the user login status.
  void SetLoginStatus(user::LoginStatus login_status);

  // Sets the current user session to be active or inactive in a desktop
  // environment.
  void SetUserInActiveDesktopEnvironment(bool is_active);

  test::UserMetricsRecorderTestAPI* user_metrics_recorder_test_api() {
    return user_metrics_recorder_test_api_.get();
  }

  base::HistogramTester& histograms() { return histograms_; }

 private:
  // Test API to access private members of the test target.
  scoped_ptr<test::UserMetricsRecorderTestAPI> user_metrics_recorder_test_api_;

  // Histogram value verifier.
  base::HistogramTester histograms_;

  // The active SystemTrayDelegate. Not owned.
  test::TestSystemTrayDelegate* test_system_tray_delegate_;

  DISALLOW_COPY_AND_ASSIGN(UserMetricsRecorderTest);
};

UserMetricsRecorderTest::UserMetricsRecorderTest() {
}

UserMetricsRecorderTest::~UserMetricsRecorderTest() {
}

void UserMetricsRecorderTest::SetUp() {
  test::AshTestBase::SetUp();
  user_metrics_recorder_test_api_.reset(new test::UserMetricsRecorderTestAPI());
  test_system_tray_delegate_ = GetSystemTrayDelegate();
}

void UserMetricsRecorderTest::TearDown() {
  test_system_tray_delegate_ = nullptr;
  test::AshTestBase::TearDown();
}

void UserMetricsRecorderTest::SetLoginStatus(user::LoginStatus login_status) {
  test_system_tray_delegate_->SetLoginStatus(login_status);
}

void UserMetricsRecorderTest::SetUserInActiveDesktopEnvironment(
    bool is_active) {
  if (is_active) {
    SetLoginStatus(user::LOGGED_IN_USER);
    ASSERT_TRUE(
        user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());
  } else {
    SetLoginStatus(user::LOGGED_IN_LOCKED);
    ASSERT_FALSE(
        user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());
  }
}

// Verifies the return value of IsUserInActiveDesktopEnvironment() for the
// different login status values.
TEST_F(UserMetricsRecorderTest, VerifyIsUserInActiveDesktopEnvironmentValues) {
  SetLoginStatus(user::LOGGED_IN_NONE);
  EXPECT_FALSE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_LOCKED);
  EXPECT_FALSE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_USER);
  EXPECT_TRUE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_OWNER);
  EXPECT_TRUE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_GUEST);
  EXPECT_TRUE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_PUBLIC);
  EXPECT_TRUE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_SUPERVISED);
  EXPECT_TRUE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());

  SetLoginStatus(user::LOGGED_IN_KIOSK_APP);
  EXPECT_FALSE(
      user_metrics_recorder_test_api()->IsUserInActiveDesktopEnvironment());
}

// Verifies that the IsUserInActiveDesktopEnvironment() dependent stats are not
// recorded when a user is not active in a desktop environment.
TEST_F(UserMetricsRecorderTest,
       VerifyStatsRecordedWhenUserNotInActiveDesktopEnvironment) {
  SetUserInActiveDesktopEnvironment(false);
  user_metrics_recorder_test_api()->RecordPeriodicMetrics();

  histograms().ExpectTotalCount(kAsh_NumberOfVisibleWindowsInPrimaryDisplay, 0);
}

// Verifies that the IsUserInActiveDesktopEnvironment() dependent stats are
// recorded when a user is active in a desktop environment.
TEST_F(UserMetricsRecorderTest,
       VerifyStatsRecordedWhenUserInActiveDesktopEnvironment) {
  SetUserInActiveDesktopEnvironment(true);
  user_metrics_recorder_test_api()->RecordPeriodicMetrics();

  histograms().ExpectTotalCount(kAsh_NumberOfVisibleWindowsInPrimaryDisplay, 1);
}

// Verifies recording of stats which are always recorded by
// RecordPeriodicMetrics.
TEST_F(UserMetricsRecorderTest, VerifyStatsRecordedByRecordPeriodicMetrics) {
  SetUserInActiveDesktopEnvironment(true);
  user_metrics_recorder_test_api()->RecordPeriodicMetrics();

  histograms().ExpectTotalCount(kAsh_ActiveWindowShowTypeOverTime, 1);
}

}  // namespace ash
