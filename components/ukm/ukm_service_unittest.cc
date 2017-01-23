// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ukm {

namespace {

class UkmServiceTest : public testing::Test {
 public:
  UkmServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());
    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmPersistedLogs);
  }

  int GetPersistedLogCount() {
    const base::ListValue* list_value =
        prefs_.GetList(prefs::kUkmPersistedLogs);
    return list_value->GetSize();
  }

 protected:
  TestingPrefServiceSimple prefs_;
  metrics::TestMetricsServiceClient client_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

 private:
  DISALLOW_COPY_AND_ASSIGN(UkmServiceTest);
};

}  // namespace

TEST_F(UkmServiceTest, EnableDisableSchedule) {
  UkmService service(&prefs_, &client_);
  EXPECT_FALSE(task_runner_->HasPendingTask());
  service.Initialize();
  EXPECT_TRUE(task_runner_->HasPendingTask());
  // Allow initialization to complete.
  task_runner_->RunUntilIdle();
  service.EnableReporting();
  EXPECT_TRUE(task_runner_->HasPendingTask());
  service.DisableReporting();
  task_runner_->RunPendingTasks();
  EXPECT_FALSE(task_runner_->HasPendingTask());
}

TEST_F(UkmServiceTest, PersistAndPurge) {
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableReporting();
  // Should init, generate a log, and unsuccessfully attempt an upload.
  task_runner_->RunPendingTasks();
  // Flushes the generated log to disk and generates a new one.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 2);
  service.Purge();
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

}  // namespace ukm
