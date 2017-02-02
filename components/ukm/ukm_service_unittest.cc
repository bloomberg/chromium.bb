// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/proto/ukm/report.pb.h"
#include "components/metrics/proto/ukm/source.pb.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/persisted_logs_metrics_impl.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

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

  Report GetPersistedReport() {
    metrics::PersistedLogs result_persisted_logs(
        base::MakeUnique<ukm::PersistedLogsMetricsImpl>(), &prefs_,
        prefs::kUkmPersistedLogs,
        3,     // log count limit
        1000,  // byte limit
        0);

    result_persisted_logs.DeserializeLogs();
    result_persisted_logs.StageLog();

    std::string uncompressed_log_data;
    EXPECT_TRUE(compression::GzipUncompress(result_persisted_logs.staged_log(),
                                            &uncompressed_log_data));

    Report report;
    EXPECT_TRUE(report.ParseFromString(uncompressed_log_data));
    return report;
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
  // Should init, generate a log, and start an upload.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Flushes the generated log to disk and generates a new one.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 2);
  service.Purge();
  EXPECT_EQ(GetPersistedLogCount(), 0);
}

TEST_F(UkmServiceTest, SourceSerialization) {
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableReporting();

  std::unique_ptr<UkmSource> source = base::WrapUnique(new UkmSource());
  source->set_committed_url(GURL("https://google.com"));
  source->set_first_contentful_paint(base::TimeDelta::FromMilliseconds(300));

  service.RecordSource(std::move(source));

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(GURL("https://google.com").spec(), proto_source.url());
  EXPECT_EQ(300, proto_source.first_contentful_paint_msec());
}

}  // namespace ukm
