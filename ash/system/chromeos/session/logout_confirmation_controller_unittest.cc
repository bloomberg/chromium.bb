// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/session/logout_confirmation_controller.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/tick_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// A SingleThreadTaskRunner that mocks the current time and allows it to be
// fast-forwarded. TODO(bartfab): Copies of this class exist in several tests.
// Consolidate them (crbug.com/329911).
class MockTimeSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  MockTimeSingleThreadTaskRunner();

  // base::SingleThreadTaskRunner:
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE;
  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE;
  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE;

  const base::TimeTicks& GetCurrentTime() const;

  void FastForwardBy(base::TimeDelta delta);
  void FastForwardUntilNoTasksRemain();

 private:
  // Strict weak temporal ordering of tasks.
  class TemporalOrder {
   public:
    bool operator()(
        const std::pair<base::TimeTicks, base::Closure>& first_task,
        const std::pair<base::TimeTicks, base::Closure>& second_task) const;
  };

  virtual ~MockTimeSingleThreadTaskRunner();

  base::TimeTicks now_;
  std::priority_queue<std::pair<base::TimeTicks, base::Closure>,
                      std::vector<std::pair<base::TimeTicks, base::Closure> >,
                      TemporalOrder> tasks_;

  DISALLOW_COPY_AND_ASSIGN(MockTimeSingleThreadTaskRunner);
};

// A base::TickClock that uses a MockTimeSingleThreadTaskRunner as the source of
// the current time.
class MockClock : public base::TickClock {
 public:
  explicit MockClock(scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner);
  virtual ~MockClock();

  // base::TickClock:
  virtual base::TimeTicks NowTicks() OVERRIDE;

 private:
  scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MockClock);
};


MockTimeSingleThreadTaskRunner::MockTimeSingleThreadTaskRunner() {
}

bool MockTimeSingleThreadTaskRunner::RunsTasksOnCurrentThread() const {
  return true;
}

bool MockTimeSingleThreadTaskRunner::PostDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  tasks_.push(std::make_pair(now_ + delay, task));
  return true;
}

bool MockTimeSingleThreadTaskRunner::PostNonNestableDelayedTask(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    base::TimeDelta delay) {
  NOTREACHED();
  return false;
}

const base::TimeTicks& MockTimeSingleThreadTaskRunner::GetCurrentTime() const {
  return now_;
}

void MockTimeSingleThreadTaskRunner::FastForwardBy(base::TimeDelta delta) {
  const base::TimeTicks latest = now_ + delta;
  while (!tasks_.empty() && tasks_.top().first <= latest) {
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
  }
  now_ = latest;
}

void MockTimeSingleThreadTaskRunner::FastForwardUntilNoTasksRemain() {
  while (!tasks_.empty()) {
    now_ = tasks_.top().first;
    base::Closure task = tasks_.top().second;
    tasks_.pop();
    task.Run();
  }
}

bool MockTimeSingleThreadTaskRunner::TemporalOrder::operator()(
    const std::pair<base::TimeTicks, base::Closure>& first_task,
    const std::pair<base::TimeTicks, base::Closure>& second_task) const {
  return first_task.first > second_task.first;
}

MockTimeSingleThreadTaskRunner::~MockTimeSingleThreadTaskRunner() {
}

MockClock::MockClock(scoped_refptr<MockTimeSingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner) {
}

MockClock::~MockClock() {
}

base::TimeTicks MockClock::NowTicks() {
  return task_runner_->GetCurrentTime();
}

}  // namespace

class LogoutConfirmationControllerTest : public testing::Test {
 protected:
  LogoutConfirmationControllerTest();
  virtual ~LogoutConfirmationControllerTest();

  void LogOut();

  bool log_out_called_;

  scoped_refptr<MockTimeSingleThreadTaskRunner> runner_;
  base::ThreadTaskRunnerHandle runner_handle_;

  LogoutConfirmationController controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LogoutConfirmationControllerTest);
};

LogoutConfirmationControllerTest::LogoutConfirmationControllerTest()
    : log_out_called_(false),
      runner_(new MockTimeSingleThreadTaskRunner),
      runner_handle_(runner_),
      controller_(base::Bind(&LogoutConfirmationControllerTest::LogOut,
                             base::Unretained(this))) {
  controller_.SetClockForTesting(
      scoped_ptr<base::TickClock>(new MockClock(runner_)));
}

LogoutConfirmationControllerTest::~LogoutConfirmationControllerTest() {
}

void LogoutConfirmationControllerTest::LogOut() {
  log_out_called_ = true;
}

// Verifies that the user is logged out immediately if logout confirmation with
// a zero-length countdown is requested.
TEST_F(LogoutConfirmationControllerTest, ZeroDuration) {
  controller_.ConfirmLogout(runner_->GetCurrentTime());
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta());
  EXPECT_TRUE(log_out_called_);
}

// Verifies that the user is logged out when the countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpired) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
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
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(30));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when a second request to confirm logout is made and the second
// request's countdown ends after the original request's, the user is logged
// out when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, DurationExtended) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the screen is locked while the countdown is running, the
// user is not logged out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, Lock) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnLockStateChanged(true);
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that when the user confirms the logout request, the user is logged
// out immediately.
TEST_F(LogoutConfirmationControllerTest, UserAccepted) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnLogoutConfirmed();
  EXPECT_TRUE(log_out_called_);
}

// Verifies that when the user denies the logout request, the user is not logged
// out, even when the original countdown expires.
TEST_F(LogoutConfirmationControllerTest, UserDenied) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);
}

// Verifies that after the user has denied a logout request, a subsequent logout
// request is handled correctly and the user is logged out when the countdown
// expires.
TEST_F(LogoutConfirmationControllerTest, DurationExpiredAfterDeniedRequest) {
  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  controller_.OnDialogClosed();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(log_out_called_);

  controller_.ConfirmLogout(
      runner_->GetCurrentTime() + base::TimeDelta::FromSeconds(10));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(9));
  EXPECT_FALSE(log_out_called_);
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(2));
  EXPECT_TRUE(log_out_called_);
}

}  // namespace ash
