// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/session_length_limiter.h"

#include <queue>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/testing_pref_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;

namespace chromeos {

namespace {

class MockSessionLengthLimiterDelegate : public SessionLengthLimiter::Delegate {
 public:
  MOCK_CONST_METHOD0(GetCurrentTime, const base::TimeTicks(void));
  MOCK_METHOD0(StopSession, void(void));
};

// A SingleThreadTaskRunner that mocks the current time and allows it to be
// fast-forwarded.
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

  void FastForwardBy(int64 milliseconds);
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
};

}  // namespace

class SessionLengthLimiterTest : public testing::Test {
 protected:
  SessionLengthLimiterTest();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  void SetSessionStartTimePref(int64 session_start_time);
  void VerifySessionStartTimePref();
  void SetSessionLengthLimitPref(int64 session_length_limit);

  void ExpectStopSession();
  void CheckStopSessionTime();

  void CreateSessionLengthLimiter(bool browser_restarted);

  TestingPrefServiceSimple local_state_;
  scoped_refptr<MockTimeSingleThreadTaskRunner> runner_;
  base::TimeTicks session_start_time_;
  base::TimeTicks session_end_time_;

  MockSessionLengthLimiterDelegate* delegate_;  // Owned by
                                                // session_length_limiter_.
  scoped_ptr<SessionLengthLimiter> session_length_limiter_;
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
  tasks_.push(std::pair<base::TimeTicks, base::Closure>(now_ + delay, task));
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

void MockTimeSingleThreadTaskRunner::FastForwardBy(int64 delta) {
  const base::TimeTicks latest =
      now_ + base::TimeDelta::FromMilliseconds(delta);
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
  return first_task.first >= second_task.first;
}

MockTimeSingleThreadTaskRunner::~MockTimeSingleThreadTaskRunner() {
}

SessionLengthLimiterTest::SessionLengthLimiterTest() : delegate_(NULL) {
}

void SessionLengthLimiterTest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  SessionLengthLimiter::RegisterPrefs(local_state_.registry());
  runner_ = new MockTimeSingleThreadTaskRunner;
  session_start_time_ = runner_->GetCurrentTime();

  delegate_ = new NiceMock<MockSessionLengthLimiterDelegate>;
  ON_CALL(*delegate_, GetCurrentTime())
      .WillByDefault(Invoke(runner_.get(),
                            &MockTimeSingleThreadTaskRunner::GetCurrentTime));
  EXPECT_CALL(*delegate_, StopSession()).Times(0);
}

void SessionLengthLimiterTest::TearDown() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
}

void SessionLengthLimiterTest::SetSessionStartTimePref(
    int64 session_start_time) {
  local_state_.SetUserPref(prefs::kSessionStartTime,
                           base::Value::CreateStringValue(
                               base::Int64ToString(session_start_time)));
}

void SessionLengthLimiterTest::VerifySessionStartTimePref() {
  base::TimeTicks session_start_time(base::TimeTicks::FromInternalValue(
      local_state_.GetInt64(prefs::kSessionStartTime)));
  EXPECT_EQ(session_start_time_, session_start_time);
}

void SessionLengthLimiterTest::SetSessionLengthLimitPref(
    int64 session_length_limit) {
  session_end_time_ = session_start_time_ +
      base::TimeDelta::FromMilliseconds(session_length_limit);
  // If the new session end time has passed already, the session should end now.
  if (session_end_time_ < runner_->GetCurrentTime())
    session_end_time_ = runner_->GetCurrentTime();
  local_state_.SetUserPref(prefs::kSessionLengthLimit,
                           base::Value::CreateIntegerValue(
                               session_length_limit));
}

void SessionLengthLimiterTest::ExpectStopSession() {
  Mock::VerifyAndClearExpectations(delegate_);
  EXPECT_CALL(*delegate_, StopSession())
    .Times(1)
    .WillOnce(Invoke(this, &SessionLengthLimiterTest::CheckStopSessionTime));
}

void SessionLengthLimiterTest::CheckStopSessionTime() {
  EXPECT_EQ(session_end_time_, runner_->GetCurrentTime());
}

void SessionLengthLimiterTest::CreateSessionLengthLimiter(
    bool browser_restarted) {
  session_length_limiter_.reset(
      new SessionLengthLimiter(delegate_, browser_restarted));
}
// Verifies that the session start time in local state is updated during login
// if no session start time has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeUnset) {
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during login
// if a session start time lying in the future has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeFuture) {
  SetSessionStartTimePref(
      (session_start_time_ + base::TimeDelta::FromHours(2)).ToInternalValue());
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during login
// if a valid session start time has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeValid) {
  SetSessionStartTimePref(
      (session_start_time_ - base::TimeDelta::FromHours(2)).ToInternalValue());
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during restart
// after a crash if no session start time has been stored before.
TEST_F(SessionLengthLimiterTest, RestartWithSessionStartTimeUnset) {
  CreateSessionLengthLimiter(true);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during restart
// after a crash if a session start time lying in the future has been stored
// before.
TEST_F(SessionLengthLimiterTest, RestartWithSessionStartTimeFuture) {
  SetSessionStartTimePref(
      (session_start_time_ + base::TimeDelta::FromHours(2)).ToInternalValue());
  CreateSessionLengthLimiter(true);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is *not* updated during
// restart after a crash if a valid session start time has been stored before.
TEST_F(SessionLengthLimiterTest, RestartWithSessionStartTimeValid) {
  session_start_time_ -= base::TimeDelta::FromHours(2);
  SetSessionStartTimePref(session_start_time_.ToInternalValue());
  CreateSessionLengthLimiter(true);
  VerifySessionStartTimePref();
}

// Creates a SessionLengthLimiter without setting a limit. Verifies that the
// limiter does not start a timer.
TEST_F(SessionLengthLimiterTest, RunWithoutSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Create a SessionLengthLimiter.
  CreateSessionLengthLimiter(false);

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a limit. Verifies that the
// limiter starts a timer and that when the session length reaches the limit,
// the session is terminated.
TEST_F(SessionLengthLimiterTest, RunWithSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(60 * 1000);  // 60 seconds.

  // Create a SessionLengthLimiter.
  CreateSessionLengthLimiter(false);

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then increases the limit to 90 seconds.
// Verifies that when the session time reaches the new 90 second limit, the
// session is terminated.
TEST_F(SessionLengthLimiterTest, RunAndIncreaseSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(60 * 1000);  // 60 seconds.

  // Create a SessionLengthLimiter.
  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(50 * 1000);  // 50 seconds.

  // Increase the session length limit to 90 seconds.
  SetSessionLengthLimitPref(90 * 1000);  // 90 seconds.

  // Verify that the the timer fires and the session is terminated when the
  // session length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then decreases the limit to 40 seconds.
// Verifies that when the limit is decreased to 40 seconds after 50 seconds of
// session time have passed, the next timer tick causes the session to be
// terminated.
TEST_F(SessionLengthLimiterTest, RunAndDecreaseSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(60 * 1000);  // 60 seconds.

  // Create a SessionLengthLimiter.
  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(50 * 1000);  // 50 seconds.

  // Verify that reducing the session length limit below the 50 seconds that
  // have already elapsed causes the session to be terminated immediately.
  ExpectStopSession();
  SetSessionLengthLimitPref(40 * 1000);  // 40 seconds.
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then removes the limit. Verifies that after
// the limit is removed, the session is not terminated when the session time
// reaches the original 60 second limit.
TEST_F(SessionLengthLimiterTest, RunAndRemoveSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(60 * 1000);  // 60 seconds.

  // Create a SessionLengthLimiter.
  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(50 * 1000);  // 50 seconds.

  // Remove the session length limit.
  local_state_.RemoveUserPref(prefs::kSessionLengthLimit);

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace chromeos
