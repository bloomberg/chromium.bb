// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_task_queue.h"

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace smb_client {
namespace {

constexpr size_t kTaskQueueCapacity = 3;
}
// SmbTaskQueue is used to test SmbTaskQueue. Tasks are added to the task queue
// with specified |task_id|'s. When a task is run by the task_queue_, it is
// added to the pending_ map and can be completed by invoking the
// CompleteTask method.
class SmbTaskQueueTest : public testing::Test {
 public:
  SmbTaskQueueTest() : task_queue_(kTaskQueueCapacity) {}
  ~SmbTaskQueueTest() override = default;

 protected:
  // Creates and adds the task with |task_id| to task_queue_.
  void CreateAndAddTask(uint32_t task_id) {
    base::OnceClosure reply =
        base::BindOnce(&SmbTaskQueueTest::OnReply, base::Unretained(this));
    SmbTask task =
        base::BindOnce(&SmbTaskQueueTest::Start, base::Unretained(this),
                       task_id, std::move(reply));

    task_queue_.AddTask(std::move(task));
  }

  // Checks whether the task |task_id| is pending.
  bool IsPending(uint32_t task_id) const { return pending_.count(task_id); }

  // Completes the pending task |task_id|, running its reply.
  void CompleteTask(uint32_t task_id) {
    DCHECK(IsPending(task_id));

    SmbTask to_run = std::move(pending_[task_id]);
    pending_.erase(task_id);

    std::move(to_run).Run();
  }

  // Returns the number of pending tasks.
  size_t PendingCount() const { return pending_.size(); }

 private:
  void OnReply() { task_queue_.TaskFinished(); }

  void Start(uint32_t task_id, base::OnceClosure reply) {
    pending_[task_id] = std::move(reply);
  }

  SmbTaskQueue task_queue_;
  std::map<uint32_t, base::OnceClosure> pending_;
  DISALLOW_COPY_AND_ASSIGN(SmbTaskQueueTest);
};

// SmbTaskQueue immediately runs a task when less than max_pending are running.
TEST_F(SmbTaskQueueTest, TaskQueueRunsASingleTask) {
  const uint32_t task_id = 1;

  CreateAndAddTask(task_id);
  EXPECT_TRUE(IsPending(task_id));

  CompleteTask(task_id);
  EXPECT_FALSE(IsPending(task_id));
}

// SmbTaskQueue runs atleast max_pending_ tasks concurrently.
TEST_F(SmbTaskQueueTest, TaskQueueRunsMultipleTasks) {
  const uint32_t task_id_1 = 1;
  const uint32_t task_id_2 = 2;
  const uint32_t task_id_3 = 3;

  CreateAndAddTask(task_id_1);
  CreateAndAddTask(task_id_2);
  CreateAndAddTask(task_id_3);

  EXPECT_EQ(kTaskQueueCapacity, PendingCount());

  EXPECT_TRUE(IsPending(task_id_1));
  EXPECT_TRUE(IsPending(task_id_2));
  EXPECT_TRUE(IsPending(task_id_3));

  CompleteTask(task_id_1);
  CompleteTask(task_id_2);
  CompleteTask(task_id_3);

  EXPECT_FALSE(IsPending(task_id_1));
  EXPECT_FALSE(IsPending(task_id_2));
  EXPECT_FALSE(IsPending(task_id_3));
}

// SmbTaskQueue runs at most max_pending_ tasks concurrently.
TEST_F(SmbTaskQueueTest, TaskQueueDoesNotRunAdditionalTestsWhenFull) {
  const uint32_t task_id_1 = 1;
  const uint32_t task_id_2 = 2;
  const uint32_t task_id_3 = 3;
  const uint32_t task_id_4 = 4;

  CreateAndAddTask(task_id_1);
  CreateAndAddTask(task_id_2);
  CreateAndAddTask(task_id_3);
  CreateAndAddTask(task_id_4);

  // The first three tasks should run.
  EXPECT_EQ(kTaskQueueCapacity, PendingCount());
  EXPECT_TRUE(IsPending(task_id_1));
  EXPECT_TRUE(IsPending(task_id_2));
  EXPECT_TRUE(IsPending(task_id_3));

  // The fourth task should wait until another task finishes to run.
  EXPECT_FALSE(IsPending(task_id_4));

  CompleteTask(task_id_1);
  EXPECT_FALSE(IsPending(task_id_1));

  // After completing a task, the fourth task should be able to run.
  EXPECT_TRUE(IsPending(task_id_4));
}

}  // namespace smb_client
}  // namespace chromeos
