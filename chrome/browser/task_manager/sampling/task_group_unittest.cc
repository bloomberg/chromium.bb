// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_group.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/test/gtest_util.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "gpu/ipc/common/memory_stats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

namespace {

class FakeTask : public Task {
 public:
  FakeTask(base::ProcessId process_id, Type type)
      : Task(base::string16(),
             "FakeTask",
             nullptr,
             base::kNullProcessHandle,
             process_id),
        type_(type) {}

  Type GetType() const override { return type_; }

  int GetChildProcessUniqueID() const override { return 0; }

  const Task* GetParentTask() const override { return nullptr; }

  int GetTabId() const override { return 0; }

 private:
  Type type_;

  DISALLOW_COPY_AND_ASSIGN(FakeTask);
};

}  // namespace

class TaskGroupTest : public testing::Test {
 public:
  TaskGroupTest()
      : io_task_runner_(content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO)),
        run_loop_(base::MakeUnique<base::RunLoop>()),
        task_group_(base::Process::Current().Handle(),
                    base::Process::Current().Pid(),
                    base::Bind(&TaskGroupTest::OnBackgroundCalculationsDone,
                               base::Unretained(this)),
                    new SharedSampler(io_task_runner_),
                    io_task_runner_),
        fake_task_(base::Process::Current().Pid(), Task::UNKNOWN) {
    // Refresh() is only valid on non-empty TaskGroups, so add a fake Task.
    task_group_.AddTask(&fake_task_);
  }

 protected:
  void OnBackgroundCalculationsDone() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
    background_refresh_complete_ = true;
    run_loop_->QuitWhenIdle();
  }

  content::TestBrowserThreadBundle browser_threads_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  std::unique_ptr<base::RunLoop> run_loop_;
  TaskGroup task_group_;
  FakeTask fake_task_;
  bool background_refresh_complete_ = false;

  DISALLOW_COPY_AND_ASSIGN(TaskGroupTest);
};

// Verify that calling TaskGroup::Refresh() without specifying any fields to
// refresh trivially completes, without crashing or leaving things in a weird
// state.
TEST_F(TaskGroupTest, NullRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0);
  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Ensure that refreshing an empty TaskGroup causes a DCHECK (if enabled).
TEST_F(TaskGroupTest, RefreshZeroTasksDeathTest) {
  // Remove the fake Task from the group.
  task_group_.RemoveTask(&fake_task_);

  EXPECT_DCHECK_DEATH(
      task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(), 0));
}

// Verify that Refresh() for a field which can be refreshed synchronously
// completes immediately, without leaving any background calculations pending.
TEST_F(TaskGroupTest, SyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_NETWORK_USAGE);
  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_FALSE(background_refresh_complete_);
}

// Some fields are refreshed on a per-TaskGroup basis, but require asynchronous
// work (e.g. on another thread) to complete. Memory is such a field, so verify
// that it is correctly reported as requiring background calculations.
TEST_F(TaskGroupTest, AsyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_MEMORY);
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
  EXPECT_TRUE(background_refresh_complete_);
}

// Some fields are refreshed system-wide, via a SharedSampler, which completes
// asynchronously. Idle wakeups are reported via SharedSampler on some systems
// and via asynchronous refresh on others, so we just test that that field
// requires background calculations, similarly to the AsyncRefresh test above.
TEST_F(TaskGroupTest, SharedAsyncRefresh) {
  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_IDLE_WAKEUPS);
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
}

// Ensure that if NaCl is enabled then calling Refresh with a NaCl Task active
// results in asynchronous completion. Also verifies that if NaCl is disabled
// then completion is synchronous.
TEST_F(TaskGroupTest, NaclRefreshWithTask) {
  FakeTask fake_task(base::Process::Current().Pid(), Task::NACL);
  task_group_.AddTask(&fake_task);

  task_group_.Refresh(gpu::VideoMemoryUsageStats(), base::TimeDelta(),
                      REFRESH_TYPE_NACL);
#if !defined(DISABLE_NACL)
  EXPECT_FALSE(task_group_.AreBackgroundCalculationsDone());

  ASSERT_FALSE(background_refresh_complete_);
  run_loop_->Run();

  EXPECT_TRUE(background_refresh_complete_);
#endif  // !defined(DISABLE_NACL)

  EXPECT_TRUE(task_group_.AreBackgroundCalculationsDone());
}

}  // namespace task_manager
