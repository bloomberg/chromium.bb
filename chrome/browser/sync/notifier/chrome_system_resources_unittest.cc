// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/notifier/chrome_system_resources.h"

#include <string>

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/tuple.h"
#include "google/cacheinvalidation/invalidation-client.h"
#include "chrome/browser/sync/notifier/state_writer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_notifier {
namespace {

using ::testing::_;
using ::testing::SaveArg;

class MockStateWriter : public StateWriter {
 public:
  MOCK_METHOD1(WriteState, void(const std::string&));
};

class MockClosure : public Callback0::Type {
 public:
  MOCK_METHOD1(RunWithParams, void(const Tuple0&));
};

class MockStorageCallback : public Callback1<bool>::Type {
 public:
  MOCK_METHOD1(RunWithParams, void(const Tuple1<bool>&));
};

class ChromeSystemResourcesTest : public testing::Test {
 protected:
  ChromeSystemResourcesTest()
      : chrome_system_resources_(&mock_state_writer_) {}

  virtual ~ChromeSystemResourcesTest() {}

  void ScheduleShouldNotRun() {
    {
      // Owned by ScheduleImmediately.
      MockClosure* should_not_run = new MockClosure();
      EXPECT_CALL(*should_not_run, RunWithParams(_)).Times(0);
      chrome_system_resources_.ScheduleImmediately(should_not_run);
    }
    {
      // Owned by ScheduleOnListenerThread.
      MockClosure* should_not_run = new MockClosure();
      EXPECT_CALL(*should_not_run, RunWithParams(_)).Times(0);
      chrome_system_resources_.ScheduleOnListenerThread(should_not_run);
    }
    {
      // Owned by ScheduleWithDelay.
      MockClosure* should_not_run = new MockClosure();
      EXPECT_CALL(*should_not_run, RunWithParams(_)).Times(0);
      chrome_system_resources_.ScheduleWithDelay(
          invalidation::TimeDelta::FromSeconds(0), should_not_run);
    }
  }

  // Needed by |chrome_system_resources_|.
  MessageLoop message_loop_;
  MockStateWriter mock_state_writer_;
  ChromeSystemResources chrome_system_resources_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSystemResourcesTest);
};

// Make sure current_time() doesn't crash or leak.
TEST_F(ChromeSystemResourcesTest, CurrentTime) {
  invalidation::Time current_time =
      chrome_system_resources_.current_time();
  VLOG(1) << "current_time returned: " << current_time.ToInternalValue();
}

// Make sure Log() doesn't crash or leak.
TEST_F(ChromeSystemResourcesTest, Log) {
  chrome_system_resources_.Log(ChromeSystemResources::INFO_LEVEL,
                               __FILE__, __LINE__, "%s %d",
                               "test string", 5);
}

TEST_F(ChromeSystemResourcesTest, ScheduleBeforeStart) {
  ScheduleShouldNotRun();
  chrome_system_resources_.StartScheduler();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAfterStop) {
  chrome_system_resources_.StartScheduler();
  chrome_system_resources_.StopScheduler();
  ScheduleShouldNotRun();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAndStop) {
  chrome_system_resources_.StartScheduler();
  ScheduleShouldNotRun();
  chrome_system_resources_.StopScheduler();
}

TEST_F(ChromeSystemResourcesTest, ScheduleAndDestroy) {
  chrome_system_resources_.StartScheduler();
  ScheduleShouldNotRun();
}

TEST_F(ChromeSystemResourcesTest, ScheduleImmediately) {
  chrome_system_resources_.StartScheduler();
  // Owned by ScheduleImmediately.
  MockClosure* mock_closure = new MockClosure();
  EXPECT_CALL(*mock_closure, RunWithParams(_));
  chrome_system_resources_.ScheduleImmediately(mock_closure);
  message_loop_.RunAllPending();
}

TEST_F(ChromeSystemResourcesTest, ScheduleOnListenerThread) {
  chrome_system_resources_.StartScheduler();
  // Owned by ScheduleOnListenerThread.
  MockClosure* mock_closure = new MockClosure();
  EXPECT_CALL(*mock_closure, RunWithParams(_));
  chrome_system_resources_.ScheduleOnListenerThread(mock_closure);
  EXPECT_FALSE(chrome_system_resources_.IsRunningOnInternalThread());
  message_loop_.RunAllPending();
}

TEST_F(ChromeSystemResourcesTest, ScheduleWithZeroDelay) {
  chrome_system_resources_.StartScheduler();
  // Owned by ScheduleWithDelay.
  MockClosure* mock_closure = new MockClosure();
  EXPECT_CALL(*mock_closure, RunWithParams(_));
  chrome_system_resources_.ScheduleWithDelay(
      invalidation::TimeDelta::FromSeconds(0), mock_closure);
  message_loop_.RunAllPending();
}

// TODO(akalin): Figure out how to test with a non-zero delay.

TEST_F(ChromeSystemResourcesTest, WriteState) {
  chrome_system_resources_.StartScheduler();
  EXPECT_CALL(mock_state_writer_, WriteState(_));
  // Owned by WriteState.
  MockStorageCallback* mock_storage_callback = new MockStorageCallback();
  Tuple1<bool> results(false);
  EXPECT_CALL(*mock_storage_callback, RunWithParams(_))
      .WillOnce(SaveArg<0>(&results));
  chrome_system_resources_.WriteState("state", mock_storage_callback);
  message_loop_.RunAllPending();
  EXPECT_TRUE(results.a);
}

}  // namespace
}  // namespace notifier
