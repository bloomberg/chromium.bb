// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "jingle/notifier/base/task_pump.h"

#include "base/message_loop.h"
#include "jingle/notifier/base/mock_task.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notifier {

namespace {

using ::testing::Return;

class TaskPumpTest : public testing::Test {
 private:
  MessageLoop message_loop_;
};

TEST_F(TaskPumpTest, Basic) {
  TaskPump task_pump;
  MockTask* task = new MockTask(&task_pump);
  // We have to do this since the state enum is protected in
  // talk_base::Task.
  const int TASK_STATE_DONE = 2;
  EXPECT_CALL(*task, ProcessStart()).WillOnce(Return(TASK_STATE_DONE));
  task->Start();

  MessageLoop::current()->RunAllPending();
}

TEST_F(TaskPumpTest, Stop) {
  TaskPump task_pump;
  MockTask* task = new MockTask(&task_pump);
  // We have to do this since the state enum is protected in
  // talk_base::Task.
  const int TASK_STATE_ERROR = 3;
  ON_CALL(*task, ProcessStart()).WillByDefault(Return(TASK_STATE_ERROR));
  EXPECT_CALL(*task, ProcessStart()).Times(0);
  task->Start();

  task_pump.Stop();
  MessageLoop::current()->RunAllPending();
}

}  // namespace

}  // namespace notifier
