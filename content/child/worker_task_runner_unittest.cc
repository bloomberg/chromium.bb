// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/worker_task_runner.h"

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "content/public/child/worker_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WorkerTaskRunnerTest : public testing::Test {
 public:
  void FakeStart() { task_runner_.DidStartWorkerRunLoop(); }
  void FakeStop() { task_runner_.WillStopWorkerRunLoop(); }
  WorkerTaskRunner task_runner_;

 private:
  base::MessageLoop message_loop_;
};

class MockObserver : public WorkerThread::Observer {
 public:
  MOCK_METHOD0(WillStopCurrentWorkerThread, void());
  void RemoveSelfOnNotify() {
    ON_CALL(*this, WillStopCurrentWorkerThread())
        .WillByDefault(testing::Invoke(this, &MockObserver::RemoveSelf));
  }
  void RemoveSelf() { WorkerThread::RemoveObserver(this); }
  WorkerTaskRunner* runner_;
};

TEST_F(WorkerTaskRunnerTest, BasicObservingAndWorkerId) {
  ASSERT_EQ(0, WorkerThread::GetCurrentId());
  MockObserver o;
  EXPECT_CALL(o, WillStopCurrentWorkerThread()).Times(1);
  FakeStart();
  WorkerThread::AddObserver(&o);
  ASSERT_LT(0, WorkerThread::GetCurrentId());
  FakeStop();
}

TEST_F(WorkerTaskRunnerTest, CanRemoveSelfDuringNotification) {
  MockObserver o;
  o.RemoveSelfOnNotify();
  o.runner_ = &task_runner_;
  EXPECT_CALL(o, WillStopCurrentWorkerThread()).Times(1);
  FakeStart();
  WorkerThread::AddObserver(&o);
  FakeStop();
}

}  // namespace content
