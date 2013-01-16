// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/test/test_pending_task.h"
#include "base/test/test_simple_task_runner.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BluetoothTaskObserver : public device::BluetoothTaskManagerWin::Observer {
 public:
  BluetoothTaskObserver() : num_updates_(0) {
  }

  virtual ~BluetoothTaskObserver() {
  }

  virtual void AdapterStateChanged(
    const device::BluetoothTaskManagerWin::AdapterState& state) {
    num_updates_++;
  }

  int num_updates() const {
    return num_updates_;
  }

 private:
   int num_updates_;
};

}  // namespace

namespace device {

class BluetoothTaskManagerWinTest : public testing::Test {
 public:
  BluetoothTaskManagerWinTest()
      : ui_task_runner_(new base::TestSimpleTaskRunner()),
        bluetooth_task_runner_(new base::TestSimpleTaskRunner()),
        task_manager_(new BluetoothTaskManagerWin(ui_task_runner_,
                                                  bluetooth_task_runner_)),
        has_bluetooth_stack_(BluetoothTaskManagerWin::HasBluetoothStack()) {
  }

  virtual void SetUp() {
    task_manager_->AddObserver(&observer_);
    task_manager_->Initialize();
  }

  virtual void TearDown() {
    task_manager_->RemoveObserver(&observer_);
  }

  int GetPollingIntervalMs() const {
    return BluetoothTaskManagerWin::kPollIntervalMs;
  }

 protected:
  scoped_refptr<base::TestSimpleTaskRunner> ui_task_runner_;
  scoped_refptr<base::TestSimpleTaskRunner> bluetooth_task_runner_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;
  BluetoothTaskObserver observer_;
  const bool has_bluetooth_stack_;
};

TEST_F(BluetoothTaskManagerWinTest, StartPolling) {
  const std::deque<base::TestPendingTask>& pending_tasks =
      bluetooth_task_runner_->GetPendingTasks();
  EXPECT_EQ(1, bluetooth_task_runner_->GetPendingTasks().size());
}

TEST_F(BluetoothTaskManagerWinTest, PollAdapterIfBluetoothStackIsAvailable) {
  bluetooth_task_runner_->RunPendingTasks();
  int num_expected_pending_tasks = has_bluetooth_stack_ ? 1 : 0;
  EXPECT_EQ(num_expected_pending_tasks,
            bluetooth_task_runner_->GetPendingTasks().size());
}

TEST_F(BluetoothTaskManagerWinTest, Polling) {
  if (!has_bluetooth_stack_) {
    return;
  }

  int expected_num_updates = 5;

  for (int i = 0; i < expected_num_updates; i++) {
    bluetooth_task_runner_->RunPendingTasks();
  }

  EXPECT_EQ(expected_num_updates, ui_task_runner_->GetPendingTasks().size());
  ui_task_runner_->RunPendingTasks();
  EXPECT_EQ(expected_num_updates, observer_.num_updates());
}

}  // namespace device