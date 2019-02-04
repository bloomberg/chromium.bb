// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/session_length_limiter.h"

#include <memory>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/prefs/testing_pref_service.h"
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

}  // namespace

class SessionLengthLimiterTest : public testing::Test {
 protected:
  SessionLengthLimiterTest();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void SetSessionUserActivitySeenPref(bool user_activity_seen);
  void ClearSessionUserActivitySeenPref();
  bool IsSessionUserActivitySeenPrefSet();
  bool GetSessionUserActivitySeenPref();

  void SetSessionStartTimePref(const base::TimeTicks& session_start_time);
  void ClearSessionStartTimePref();
  bool IsSessionStartTimePrefSet();
  base::TimeTicks GetSessionStartTimePref();

  void SetSessionLengthLimitPref(const base::TimeDelta& session_length_limit);
  void ClearSessionLengthLimitPref();

  void SetWaitForInitialUserActivityPref(bool wait_for_initial_user_activity);

  void SimulateUserActivity();

  void UpdateSessionStartTimeIfWaitingForUserActivity();

  void ExpectStopSession();
  void SaveSessionStopTime();

  // Clears the session state by resetting |user_activity_| and
  // |session_start_time_| and creates a new SessionLengthLimiter.
  void CreateSessionLengthLimiter(bool browser_restarted);

  void DestroySessionLengthLimiter();

  scoped_refptr<base::TestMockTimeTaskRunner> runner_;
  base::TimeTicks session_start_time_;
  base::TimeTicks session_stop_time_;

 private:
  TestingPrefServiceSimple local_state_;
  bool user_activity_seen_;

  MockSessionLengthLimiterDelegate* delegate_;  // Owned by
                                                // session_length_limiter_.
  std::unique_ptr<SessionLengthLimiter> session_length_limiter_;
};

SessionLengthLimiterTest::SessionLengthLimiterTest()
    : user_activity_seen_(false),
      delegate_(NULL) {
}

void SessionLengthLimiterTest::SetUp() {
  TestingBrowserProcess::GetGlobal()->SetLocalState(&local_state_);
  SessionLengthLimiter::RegisterPrefs(local_state_.registry());
  runner_ = new base::TestMockTimeTaskRunner;
  runner_->FastForwardBy(base::TimeDelta::FromInternalValue(1000));
}

void SessionLengthLimiterTest::TearDown() {
  session_length_limiter_.reset();
  TestingBrowserProcess::GetGlobal()->SetLocalState(NULL);
}

void SessionLengthLimiterTest::SetSessionUserActivitySeenPref(
    bool user_activity_seen) {
  local_state_.SetUserPref(prefs::kSessionUserActivitySeen,
                           std::make_unique<base::Value>(user_activity_seen));
}

void SessionLengthLimiterTest::ClearSessionUserActivitySeenPref() {
  local_state_.ClearPref(prefs::kSessionUserActivitySeen);
}

bool SessionLengthLimiterTest::IsSessionUserActivitySeenPrefSet() {
  return local_state_.HasPrefPath(prefs::kSessionUserActivitySeen);
}

bool SessionLengthLimiterTest::GetSessionUserActivitySeenPref() {
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  return local_state_.GetBoolean(prefs::kSessionUserActivitySeen);
}

void SessionLengthLimiterTest::SetSessionStartTimePref(
    const base::TimeTicks& session_start_time) {
  local_state_.SetUserPref(prefs::kSessionStartTime,
                           std::make_unique<base::Value>(base::NumberToString(
                               session_start_time.ToInternalValue())));
}

void SessionLengthLimiterTest::ClearSessionStartTimePref() {
  local_state_.ClearPref(prefs::kSessionStartTime);
}

bool SessionLengthLimiterTest::IsSessionStartTimePrefSet() {
  return local_state_.HasPrefPath(prefs::kSessionStartTime);
}

base::TimeTicks SessionLengthLimiterTest::GetSessionStartTimePref() {
  EXPECT_TRUE(IsSessionStartTimePrefSet());
  return base::TimeTicks::FromInternalValue(
      local_state_.GetInt64(prefs::kSessionStartTime));
}

void SessionLengthLimiterTest::SetSessionLengthLimitPref(
    const base::TimeDelta& session_length_limit) {
  local_state_.SetUserPref(prefs::kSessionLengthLimit,
                           std::make_unique<base::Value>(static_cast<int>(
                               session_length_limit.InMilliseconds())));
  UpdateSessionStartTimeIfWaitingForUserActivity();
}

void SessionLengthLimiterTest::ClearSessionLengthLimitPref() {
  local_state_.RemoveUserPref(prefs::kSessionLengthLimit);
  UpdateSessionStartTimeIfWaitingForUserActivity();
}

void SessionLengthLimiterTest::SetWaitForInitialUserActivityPref(
    bool wait_for_initial_user_activity) {
  UpdateSessionStartTimeIfWaitingForUserActivity();
  local_state_.SetUserPref(
      prefs::kSessionWaitForInitialUserActivity,
      std::make_unique<base::Value>(wait_for_initial_user_activity));
}

void SessionLengthLimiterTest::SimulateUserActivity() {
  if (session_length_limiter_)
    session_length_limiter_->OnUserActivity(NULL);
  UpdateSessionStartTimeIfWaitingForUserActivity();
  user_activity_seen_ = true;
}

void SessionLengthLimiterTest::
    UpdateSessionStartTimeIfWaitingForUserActivity() {
  if (!user_activity_seen_ &&
      local_state_.GetBoolean(prefs::kSessionWaitForInitialUserActivity)) {
    session_start_time_ = runner_->NowTicks();
  }
}

void SessionLengthLimiterTest::ExpectStopSession() {
  Mock::VerifyAndClearExpectations(delegate_);
  EXPECT_CALL(*delegate_, StopSession())
      .Times(1)
      .WillOnce(Invoke(this, &SessionLengthLimiterTest::SaveSessionStopTime));
}

void SessionLengthLimiterTest::SaveSessionStopTime() {
  session_stop_time_ = runner_->NowTicks();
}

void SessionLengthLimiterTest::CreateSessionLengthLimiter(
    bool browser_restarted) {
  user_activity_seen_ = false;
  session_start_time_ = runner_->NowTicks();

  EXPECT_FALSE(delegate_);
  delegate_ = new NiceMock<MockSessionLengthLimiterDelegate>;
  ON_CALL(*delegate_, GetCurrentTime())
      .WillByDefault(
          Invoke(runner_.get(), &base::TestMockTimeTaskRunner::NowTicks));
  EXPECT_CALL(*delegate_, StopSession()).Times(0);
  session_length_limiter_.reset(
      new SessionLengthLimiter(delegate_, browser_restarted));
}

void SessionLengthLimiterTest::DestroySessionLengthLimiter() {
  session_length_limiter_.reset();
  delegate_ = NULL;
}

// Verifies that when not instructed to wait for initial user activity, the
// session start time is set and the pref indicating user activity is cleared
// in local state during login.
TEST_F(SessionLengthLimiterTest, StartDoNotWaitForInitialUserActivity) {
  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(session_start_time_ + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(session_start_time_ + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(session_start_time_ - base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(session_start_time_ - base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that when instructed to wait for initial user activity, the session
// start time and the pref indicating user activity are cleared in local state
// during login.
TEST_F(SessionLengthLimiterTest, StartWaitForInitialUserActivity) {
   SetWaitForInitialUserActivityPref(true);

  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->NowTicks() - base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->NowTicks() - base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();
}

// Verifies that when not instructed to wait for initial user activity, local
// state is correctly updated during restart after a crash:
// * If no valid session start time is found in local state, the session start
//   time is set and the pref indicating user activity is cleared.
// * If a valid session start time is found in local state, the session start
//   time and the pref indicating user activity are *not* modified.
TEST_F(SessionLengthLimiterTest, RestartDoNotWaitForInitialUserActivity) {
  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  const base::TimeTicks stored_session_start_time =
      runner_->NowTicks() - base::TimeDelta::FromHours(2);

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that when instructed to wait for initial user activity, local state
// is correctly updated during restart after a crash:
// * If no valid session start time is found in local state, the session start
//   time and the pref indicating user activity are cleared.
// * If a valid session start time is found in local state, the session start
//   time and the pref indicating user activity are *not* modified.
TEST_F(SessionLengthLimiterTest, RestartWaitForInitialUserActivity) {
  SetWaitForInitialUserActivityPref(true);

  // Pref indicating user activity not set. Session start time not set.
  ClearSessionUserActivitySeenPref();
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time not set.
  SetSessionUserActivitySeenPref(true);
  ClearSessionStartTimePref();
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity not set. Session start time in the future.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time in the future.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(runner_->NowTicks() + base::TimeDelta::FromHours(2));
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());
  DestroySessionLengthLimiter();

  const base::TimeTicks stored_session_start_time =
      runner_->NowTicks() - base::TimeDelta::FromHours(2);

  // Pref indicating user activity not set. Session start time valid.
  ClearSessionUserActivitySeenPref();
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();

  // Pref indicating user activity set. Session start time valid.
  SetSessionUserActivitySeenPref(true);
  SetSessionStartTimePref(stored_session_start_time);
  CreateSessionLengthLimiter(true);
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(stored_session_start_time, GetSessionStartTimePref());
  DestroySessionLengthLimiter();
}

// Verifies that local state is correctly updated when waiting for initial user
// activity is toggled and no user activity has occurred yet.
TEST_F(SessionLengthLimiterTest, ToggleWaitForInitialUserActivity) {
  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Enable waiting for initial user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SetWaitForInitialUserActivityPref(true);

  // Verify that the session start time was cleared and the pref indicating user
  // activity was not set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Disable waiting for initial user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SetWaitForInitialUserActivityPref(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Verifies that local state is correctly updated when instructed not to wait
// for initial user activity and user activity occurs. Also verifies that once
// initial user activity has occurred, neither the session start time nor the
// pref indicating user activity change in local state anymore.
TEST_F(SessionLengthLimiterTest, UserActivityWhileNotWaiting) {
  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity was not set and the session
  // start time was set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were set.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Enable waiting for initial user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SetWaitForInitialUserActivityPref(true);

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Verifies that local state is correctly updated when instructed to wait for
// initial user activity and user activity occurs. Also verifies that once
// initial user activity has occurred, neither the session start time nor the
// pref indicating user activity change in local state anymore.
TEST_F(SessionLengthLimiterTest, UserActivityWhileWaiting) {
  SetWaitForInitialUserActivityPref(true);

  CreateSessionLengthLimiter(false);

  // Verify that the pref indicating user activity and the session start time
  // were not set.
  EXPECT_FALSE(IsSessionUserActivitySeenPrefSet());
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Simulate user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were set.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SimulateUserActivity();

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Disable waiting for initial user activity.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(1));
  SetWaitForInitialUserActivityPref(false);

  // Verify that the pref indicating user activity and the session start time
  // were not changed.
  EXPECT_TRUE(IsSessionUserActivitySeenPrefSet());
  EXPECT_TRUE(GetSessionUserActivitySeenPref());
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());
}

// Creates a SessionLengthLimiter without setting a limit. Verifies that the
// limiter does not start a timer.
TEST_F(SessionLengthLimiterTest, RunWithoutLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  CreateSessionLengthLimiter(false);

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a limit and instructs it not to
// wait for user activity. Verifies that the limiter starts a timer even if no
// user activity occurs and that when the session length reaches the limit, the
// session is terminated.
TEST_F(SessionLengthLimiterTest, RunWithoutUserActivityWhileNotWaiting) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(session_start_time_ + base::TimeDelta::FromSeconds(60),
            session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a limit and instructs it to wait
// for initial user activity. Verifies that if no user activity occurs, the
// limiter does not start a timer.
TEST_F(SessionLengthLimiterTest, RunWithoutUserActivityWhileWaiting) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);
  SetWaitForInitialUserActivityPref(true);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

// Creates a SessionLengthLimiter after setting a limit and instructs it not to
// wait for user activity. Verifies that the limiter starts a timer and that
// when the session length reaches the limit, the session is terminated. Also
// verifies that user activity does not affect the timer.
TEST_F(SessionLengthLimiterTest, RunWithUserActivityWhileNotWaiting) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(session_start_time_ + base::TimeDelta::FromSeconds(60),
            session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a limit and instructs it to wait
// for initial user activity. Verifies that once user activity occurs, the
// limiter starts a timer and that when the session length reaches the limit,
// the session is terminated. Also verifies that further user activity does not
// affect the timer.
TEST_F(SessionLengthLimiterTest, RunWithUserActivityWhileWaiting) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);
  SetWaitForInitialUserActivityPref(true);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);
  EXPECT_FALSE(IsSessionStartTimePrefSet());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Simulate user activity after 20 seconds.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(20));
  SimulateUserActivity();
  EXPECT_EQ(session_start_time_, GetSessionStartTimePref());

  // Verify that the timer fires and the session is terminated when the session
  // length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(session_start_time_ + base::TimeDelta::FromSeconds(60),
            session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then increases the limit to 90 seconds.
// Verifies that when the session time reaches the new 90 second limit, the
// session is terminated.
TEST_F(SessionLengthLimiterTest, RunAndIncreaseSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(50));

  // Increase the session length limit to 90 seconds.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(90));

  // Verify that the the timer fires and the session is terminated when the
  // session length limit is reached.
  ExpectStopSession();
  runner_->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(session_start_time_ + base::TimeDelta::FromSeconds(90),
            session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then decreases the limit to 40 seconds.
// Verifies that when the limit is decreased to 40 seconds after 50 seconds of
// session time have passed, the next timer tick causes the session to be
// terminated.
TEST_F(SessionLengthLimiterTest, RunAndDecreaseSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(50));

  // Verify that reducing the session length limit below the 50 seconds that
  // have already elapsed causes the session to be terminated immediately.
  ExpectStopSession();
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(40));
  EXPECT_EQ(session_start_time_ + base::TimeDelta::FromSeconds(50),
            session_stop_time_);
}

// Creates a SessionLengthLimiter after setting a 60 second limit, allows 50
// seconds of session time to pass, then removes the limit. Verifies that after
// the limit is removed, the session is not terminated when the session time
// reaches the original 60 second limit.
TEST_F(SessionLengthLimiterTest, RunAndRemoveSessionLengthLimit) {
  base::ThreadTaskRunnerHandle runner_handler(runner_);

  // Set a 60 second session time limit.
  SetSessionLengthLimitPref(base::TimeDelta::FromSeconds(60));

  CreateSessionLengthLimiter(false);

  // Fast forward the time by 50 seconds, verifying that no timer fires to
  // terminate the session.
  runner_->FastForwardBy(base::TimeDelta::FromSeconds(50));

  // Remove the session length limit.
  ClearSessionLengthLimitPref();

  // Verify that no timer fires to terminate the session.
  runner_->FastForwardUntilNoTasksRemain();
}

}  // namespace chromeos
