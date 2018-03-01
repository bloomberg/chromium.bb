// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/prefetch_background_task_handler_impl.h"

#include "base/test/test_mock_time_task_runner.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "net/base/backoff_entry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

class PrefetchBackgroundTaskHandlerImplTest : public testing::Test {
 public:
  PrefetchBackgroundTaskHandlerImplTest();
  ~PrefetchBackgroundTaskHandlerImplTest() override;

  void SetUp() override;

  PrefetchBackgroundTaskHandlerImpl* task_handler() {
    return task_handler_.get();
  }
  int64_t BackoffTime() {
    return task_handler()->GetAdditionalBackoffSeconds();
  }

  std::unique_ptr<PrefetchBackgroundTaskHandlerImpl> CreateHandler() {
    auto result = std::make_unique<PrefetchBackgroundTaskHandlerImpl>(
        profile_.GetPrefs());
    result->SetTickClockForTesting(clock_.get());
    return result;
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_refptr<base::TestMockTimeTaskRunner> task_runner_;
  std::unique_ptr<base::TickClock> clock_;
  std::unique_ptr<PrefetchBackgroundTaskHandlerImpl> task_handler_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefetchBackgroundTaskHandlerImplTest);
};

PrefetchBackgroundTaskHandlerImplTest::PrefetchBackgroundTaskHandlerImplTest()
    : task_runner_(new base::TestMockTimeTaskRunner(base::Time::Now(),
                                                    base::TimeTicks::Now())),
      clock_(task_runner_->GetMockTickClock()) {
  task_handler_ = CreateHandler();
}

PrefetchBackgroundTaskHandlerImplTest::
    ~PrefetchBackgroundTaskHandlerImplTest() {}

void PrefetchBackgroundTaskHandlerImplTest::SetUp() {}

TEST_F(PrefetchBackgroundTaskHandlerImplTest, Backoff) {
  EXPECT_EQ(0, task_handler()->GetAdditionalBackoffSeconds());

  task_handler()->Backoff();
  EXPECT_GT(task_handler()->GetAdditionalBackoffSeconds(), 0);

  // Reset the task handler to ensure it lasts across restarts.
  task_handler_ = CreateHandler();
  EXPECT_GT(task_handler()->GetAdditionalBackoffSeconds(), 0);

  task_handler()->ResetBackoff();
  EXPECT_EQ(0, task_handler()->GetAdditionalBackoffSeconds());
}

}  // namespace offline_pages
