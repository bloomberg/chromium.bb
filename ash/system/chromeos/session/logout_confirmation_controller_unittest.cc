// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/logout_confirmation_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class LogoutConfirmationControllerTest : public testing::Test {
 protected:
  LogoutConfirmationControllerTest();
  ~LogoutConfirmationControllerTest() override;

  void LogOut();

  bool log_out_called_;

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;

  LogoutConfirmationController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationControllerTest);
};

LogoutConfirmationControllerTest::LogoutConfirmationControllerTest()
    : log_out_called_(false),
      runner_(new base::TestMockTimeTaskRunner),
      runner_handle_(runner_),
      controller_(base::Bind(&LogoutConfirmationControllerTest::LogOut,
                             base::Unretained(this))) {
  controller_.SetClockForTesting(runner_->GetMockTickClock());
}

LogoutConfirmationControllerTest::~LogoutConfirmationControllerTest() {
}

void LogoutConfirmationControllerTest::LogOut() {
  log_out_called_ = true;
}

// Verifies that the user is logged out immediately if logout confirmation with
// a zero-length countdown is requested.
TEST_F(LogoutConfirmationControllerTest, ZeroDuration) {
  controller_.ConfirmLogout(runner_->NowTicks());
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(log_out_called_);
}

// Verifies that the user is logged out when the countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpired) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends before the original request's, the user is logged
// out when the new countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationShortened) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(30));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends after the original request's, the user is logged
// out when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExtended) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the screen is locked while the countdown is running, the
// user is not logged out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, Lock) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnLockStateChanged(true);
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that when the user confirms the logout request, the user is logged
// out immediately.
TEST_F(LogoutConfirmationControllerTest, UserAccepted) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnLogoutConfirmed();
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the user denies the logout request, the user is not logged
// out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, UserDenied) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that after the user has denied a logout request, a subsequent logout
// request is handled correctly and the user is logged out when the countdown
// expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpiredAfterDeniedRequest) {
  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);

  controller_.ConfirmLogout(runner_->NowTicks() +
                            base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

}  // namespace ash
