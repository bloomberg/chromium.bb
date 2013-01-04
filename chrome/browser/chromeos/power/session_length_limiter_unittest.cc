// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/session_length_limiter.h"

#include <deque>
#include <utility>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/string_number_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;

namespace chromeos {

namespace {

// The interval at which the SessionLengthLimiter checks whether the remaining
// session time has reachzed zero.
const base::TimeDelta kSessionLengthLimitTimerInterval(
    base::TimeDelta::FromSeconds(1));

const base::TimeDelta kZeroTimeDelta;
const base::TimeDelta kTenSeconds(base::TimeDelta::FromSeconds(10));

class MockSessionLengthLimiterDelegate : public SessionLengthLimiter::Delegate {
 public:
  MOCK_CONST_METHOD0(GetCurrentTime, const base::Time(void));
  MOCK_METHOD0(StopSession, void(void));
};

// A SingleThreadTaskRunner that allows the task queue to be inspected and
// delayed tasks to be run without waiting for the actual delays to expire.
class ImmediateSingleThreadTaskRunner : public base::SingleThreadTaskRunner {
 public:
  virtual bool RunsTasksOnCurrentThread() const OVERRIDE {
    return true;
  }

  virtual bool PostDelayedTask(const tracked_objects::Location& from_here,
                               const base::Closure& task,
                               base::TimeDelta delay) OVERRIDE {
    tasks_.push_back(std::pair<base::TimeDelta, base::Closure>(delay, task));
    return true;
  }

  virtual bool PostNonNestableDelayedTask(
      const tracked_objects::Location& from_here,
      const base::Closure& task,
      base::TimeDelta delay) OVERRIDE {
    NOTREACHED();
    return false;
  }

  void RunTasks() {
    std::deque<std::pair<base::TimeDelta, base::Closure> > tasks;
    tasks.swap(tasks_);
    for (std::deque<std::pair<base::TimeDelta, base::Closure> >::iterator
             it = tasks.begin(); it != tasks.end(); ++it) {
      it->second.Run();
    }
  }

  const std::deque<std::pair<base::TimeDelta, base::Closure> >& tasks() const {
    return tasks_;
  }

 private:
  std::deque<std::pair<base::TimeDelta, base::Closure> > tasks_;

  virtual ~ImmediateSingleThreadTaskRunner() {}
};

}  // namespace

class SessionLengthLimiterTest : public testing::Test {
 protected:
  SessionLengthLimiterTest() : delegate_(NULL) {
  }

  virtual void SetUp() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
    SessionLengthLimiter::RegisterPrefs(&local_state_);

    delegate_ = new NiceMock<MockSessionLengthLimiterDelegate>;
    ON_CALL(*delegate_, GetCurrentTime())
        .WillByDefault(Invoke(this, &SessionLengthLimiterTest::GetCurrentTime));
    EXPECT_CALL(*delegate_, StopSession()).Times(0);
    runner_ = new ImmediateSingleThreadTaskRunner;

    // Initialize the mock clock to a fixed value, ensuring that timezone
    // differences or DST changes do not affect the test.
    now_ = base::Time::UnixEpoch() + base::TimeDelta::FromDays(40 * 365);
    session_start_time_ = now_;
  }

  virtual void TearDown() {
    TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
  }

  void SetSessionStartTimePref(int64 session_start_time) {
    local_state_.SetUserPref(prefs::kSessionStartTime,
                             base::Value::CreateStringValue(
                                 base::Int64ToString(session_start_time)));
  }

  void VerifySessionStartTimePref() {
    base::Time session_start_time(base::Time::FromInternalValue(
        local_state_.GetInt64(prefs::kSessionStartTime)));
    EXPECT_EQ(session_start_time_, session_start_time);
  }

  void SetSessionLengthLimitPref(int64 session_length_limit) {
    local_state_.SetUserPref(prefs::kSessionLengthLimit,
                             base::Value::CreateIntegerValue(
                                 session_length_limit));
    base::TimeDelta remaining(
          base::TimeDelta::FromMilliseconds(session_length_limit) -
              (now_ - session_start_time_));
      if (remaining < kZeroTimeDelta)
        remaining = kZeroTimeDelta;
    remaining_.reset(new base::TimeDelta(remaining));
  }

  void ExpectStopSession() {
    Mock::VerifyAndClearExpectations(delegate_);
    EXPECT_CALL(*delegate_, StopSession()).Times(1);
  }

  void CreateSessionLengthLimiter(bool browser_restarted) {
    session_length_limiter_.reset(
        new SessionLengthLimiter(delegate_, browser_restarted));
  }

  void VerifyNoTimerTickIsEnqueued() {
    EXPECT_TRUE(runner_->tasks().empty());
  }

  void VerifyTimerTickIsEnqueued() {
    ASSERT_EQ(1U, runner_->tasks().size());
    EXPECT_EQ(kSessionLengthLimitTimerInterval,
              runner_->tasks().front().first);
  }

  void VerifyTimerTick() {
    VerifyTimerTickIsEnqueued();
    runner_->RunTasks();

    now_ += kSessionLengthLimitTimerInterval;
    *remaining_ -= kSessionLengthLimitTimerInterval;
    if (*remaining_ < kZeroTimeDelta)
      remaining_.reset(new base::TimeDelta(kZeroTimeDelta));
  }

  base::Time GetCurrentTime() const {
    return now_;
  }

  TestingPrefServiceSimple local_state_;
  MockSessionLengthLimiterDelegate* delegate_;  // Owned by
                                                // session_length_limiter_.
  scoped_refptr<ImmediateSingleThreadTaskRunner> runner_;

  base::Time session_start_time_;
  base::Time now_;
  scoped_ptr<base::TimeDelta> remaining_;

  scoped_ptr<SessionLengthLimiter> session_length_limiter_;
};

// Verifies that the session start time in local state is updated during login
// if no session start time has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeUnset) {
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during login
// if an invalid session start time has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeInvalid) {
  SetSessionStartTimePref(0);
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during login
// if a session start time lying in the future has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeFuture) {
  SetSessionStartTimePref(
      (now_ + base::TimeDelta::FromHours(2)).ToInternalValue());
  CreateSessionLengthLimiter(false);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during login
// if a valid session start time has been stored before.
TEST_F(SessionLengthLimiterTest, StartWithSessionStartTimeValid) {
  const base::Time previous_start_time = now_ - base::TimeDelta::FromHours(2);
  SetSessionStartTimePref(previous_start_time.ToInternalValue());
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
// after a crash if an invalid session start time has been stored before.
TEST_F(SessionLengthLimiterTest, RestartWithSessionStartTimeInvalid) {
  SetSessionStartTimePref(0);
  CreateSessionLengthLimiter(true);
  VerifySessionStartTimePref();
}

// Verifies that the session start time in local state is updated during restart
// after a crash if a session start time lying in the future has been stored
// before.
TEST_F(SessionLengthLimiterTest, RestartWithSessionStartTimeFuture) {
  SetSessionStartTimePref(
      (now_ + base::TimeDelta::FromHours(2)).ToInternalValue());
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

  // Verify that no timer tick has been enqueued.
  VerifyNoTimerTickIsEnqueued();
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

  // Check timer ticks until the remaining session time reaches zero.
  while (*remaining_ > kZeroTimeDelta)
    VerifyTimerTick();

  // Check that the next timer tick leads to the session being terminated.
  ExpectStopSession();
  VerifyTimerTick();
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

  // Check timer ticks for 50 seconds of session time.
  while (*remaining_ > kTenSeconds)
    VerifyTimerTick();

  // Increase the session length limit to 90 seconds.
  SetSessionLengthLimitPref(90 * 1000);  // 90 seconds.

  // Check timer ticks until the remaining session time reaches zero.
  while (*remaining_  > kZeroTimeDelta)
    VerifyTimerTick();

  // Check that the next timer tick leads to the session being terminated.
  ExpectStopSession();
  VerifyTimerTick();
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

  // Check timer ticks for 50 seconds of session time.
  while (*remaining_ > kTenSeconds)
    VerifyTimerTick();

  // Reduce the session length limit below the 50 seconds that have already
  // elapsed.
  SetSessionLengthLimitPref(40 * 1000);  // 40 seconds.

  // Check that the next timer tick causes the session to be terminated.
  ExpectStopSession();
  VerifyTimerTick();
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

  // Check timer ticks for 50 seconds of session time.
  while (*remaining_ > kTenSeconds)
    VerifyTimerTick();

  // Remove the session length limit.
  local_state_.RemoveUserPref(prefs::kSessionLengthLimit);

  // Continue advancing the session time until it reaches the original 60 second
  // limit.
  while (*remaining_ > kZeroTimeDelta) {
    runner_->RunTasks();

    now_ += kSessionLengthLimitTimerInterval;
    *remaining_ -= kSessionLengthLimitTimerInterval;
    if (*remaining_ < kZeroTimeDelta)
      remaining_.reset(new base::TimeDelta(kZeroTimeDelta));
  }

  // Check that the next timer tick does not lead to the session being
  // terminated.
  now_ += kSessionLengthLimitTimerInterval;
  runner_->RunTasks();
}

}  // namespace chromeos
