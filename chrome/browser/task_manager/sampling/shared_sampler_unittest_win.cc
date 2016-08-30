// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/shared_sampler.h"

#include <windows.h>

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace task_manager {

// This test class drives SharedSampler in a way similar to the real
// implementation in TaskManagerImpl and TaskGroup.
class SharedSamplerTest : public testing::Test {
 public:
  SharedSamplerTest()
    : blocking_pool_runner_(GetBlockingPoolRunner()),
      shared_sampler_(new SharedSampler(blocking_pool_runner_)) {
    shared_sampler_->RegisterCallbacks(
        base::GetCurrentProcId(),
        base::Bind(&SharedSamplerTest::OnIdleWakeupsRefreshDone,
                   base::Unretained(this)),
        base::Bind(&SharedSamplerTest::OnPhysicalMemoryUsageRefreshDone,
                   base::Unretained(this)));
   }

  ~SharedSamplerTest() override {}

 protected:
  int64_t physical_bytes() const { return physical_bytes_; }

  int idle_wakeups_per_second() const { return idle_wakeups_per_second_; }

  int64_t finished_refresh_type() const { return finished_refresh_type_; }

  void StartRefresh(int64_t refresh_flags) {
    finished_refresh_type_ = 0;
    expected_refresh_type_ = refresh_flags;
    shared_sampler_->Refresh(base::GetCurrentProcId(), refresh_flags);
  }

  void WaitUntilRefreshDone() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitWhenIdleClosure();
    run_loop.Run();
  }

 private:
  static scoped_refptr<base::SequencedTaskRunner> GetBlockingPoolRunner() {
    base::SequencedWorkerPool* blocking_pool =
        content::BrowserThread::GetBlockingPool();
    return blocking_pool->GetSequencedTaskRunner(
        blocking_pool->GetSequenceToken());
  }

  void OnRefreshTypeFinished(int64_t finished_refresh_type) {
    finished_refresh_type_ |= finished_refresh_type;

    if (finished_refresh_type_ == expected_refresh_type_)
      quit_closure_.Run();
  }

  void OnPhysicalMemoryUsageRefreshDone(int64_t physical_bytes) {
    physical_bytes_ = physical_bytes;
    OnRefreshTypeFinished(REFRESH_TYPE_PHYSICAL_MEMORY);
  }

  void OnIdleWakeupsRefreshDone(int idle_wakeups_per_second) {
    idle_wakeups_per_second_ = idle_wakeups_per_second;
    OnRefreshTypeFinished(REFRESH_TYPE_IDLE_WAKEUPS);
  }

  int64_t expected_refresh_type_ = 0;
  int64_t finished_refresh_type_ = 0;
  base::Closure quit_closure_;

  int64_t physical_bytes_ = 0;
  int idle_wakeups_per_second_ = -1;

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;
  scoped_refptr<SharedSampler> shared_sampler_;

  DISALLOW_COPY_AND_ASSIGN(SharedSamplerTest);
};

// Tests that Idle Wakeups per second value can be obtained from SharedSampler.
TEST_F(SharedSamplerTest, IdleWakeups) {
  StartRefresh(REFRESH_TYPE_IDLE_WAKEUPS);
  WaitUntilRefreshDone();
  EXPECT_EQ(REFRESH_TYPE_IDLE_WAKEUPS, finished_refresh_type());

  // Since idle_wakeups_per_second is an incremental value that reflects delta
  // between two refreshes, the first refresh always returns zero value. This
  // is consistent with implementation on other platforms.
  EXPECT_EQ(0, idle_wakeups_per_second());

  // Do a short sleep - that should trigger a context switch.
  ::Sleep(1);

  // Do another refresh.
  StartRefresh(REFRESH_TYPE_IDLE_WAKEUPS);
  WaitUntilRefreshDone();

  // Should get a greater than zero rate now.
  EXPECT_GT(idle_wakeups_per_second(), 0);
}

// Verifies that Memory (Private WS) value can be obtained from Shared Sampler.
TEST_F(SharedSamplerTest, PhysicalMemory) {
  StartRefresh(REFRESH_TYPE_PHYSICAL_MEMORY);
  WaitUntilRefreshDone();
  EXPECT_EQ(REFRESH_TYPE_PHYSICAL_MEMORY, finished_refresh_type());

  int64_t initial_value = physical_bytes();

  // Allocate a large continuos block of memory.
  const int allocated_size = 4 * 1024 * 1024;
  std::vector<uint8_t> memory_block(allocated_size);

  StartRefresh(REFRESH_TYPE_PHYSICAL_MEMORY);
  WaitUntilRefreshDone();

  // Verify that physical bytes has increased accordingly.
  EXPECT_GE(physical_bytes(), initial_value + allocated_size);
}

// Verifies that multiple refresh types can be refreshed at the same time.
TEST_F(SharedSamplerTest, MultipleRefreshTypes) {
  StartRefresh(REFRESH_TYPE_IDLE_WAKEUPS | REFRESH_TYPE_PHYSICAL_MEMORY);
  WaitUntilRefreshDone();
  EXPECT_EQ(REFRESH_TYPE_IDLE_WAKEUPS | REFRESH_TYPE_PHYSICAL_MEMORY,
            finished_refresh_type());
}

}  // namespace task_manager
