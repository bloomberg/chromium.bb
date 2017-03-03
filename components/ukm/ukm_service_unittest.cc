// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ukm/ukm_service.h"

#include <map>
#include <string>
#include <utility>

#include "base/hash.h"
#include "base/metrics/metrics_hashes.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/metrics/proto/ukm/report.pb.h"
#include "components/metrics/proto/ukm/source.pb.h"
#include "components/metrics/test_metrics_provider.h"
#include "components/metrics/test_metrics_service_client.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/persisted_logs_metrics_impl.h"
#include "components/ukm/ukm_entry_builder.h"
#include "components/ukm/ukm_pref_names.h"
#include "components/ukm/ukm_source.h"
#include "components/variations/variations_associated_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/zlib/google/compression_utils.h"

namespace ukm {

namespace {

// TODO(rkaplow): consider making this a generic testing class in
// components/variations.
class ScopedUkmFeatureParams {
 public:
  ScopedUkmFeatureParams(
      base::FeatureList::OverrideState feature_state,
      const std::map<std::string, std::string>& variation_params) {
    static const char kTestFieldTrialName[] = "TestTrial";
    static const char kTestExperimentGroupName[] = "TestGroup";

    variations::testing::ClearAllVariationParams();

    EXPECT_TRUE(variations::AssociateVariationParams(
        kTestFieldTrialName, kTestExperimentGroupName, variation_params));

    base::FieldTrial* field_trial = base::FieldTrialList::CreateFieldTrial(
        kTestFieldTrialName, kTestExperimentGroupName);

    std::unique_ptr<base::FeatureList> feature_list(new base::FeatureList);
    feature_list->RegisterFieldTrialOverride(kUkmFeature.name, feature_state,
                                             field_trial);

    // Since we are adding a scoped feature list after browser start, copy over
    // the existing feature list to prevent inconsistency.
    base::FeatureList* existing_feature_list = base::FeatureList::GetInstance();
    if (existing_feature_list) {
      std::string enabled_features;
      std::string disabled_features;
      base::FeatureList::GetInstance()->GetFeatureOverrides(&enabled_features,
                                                            &disabled_features);
      feature_list->InitializeFromCommandLine(enabled_features,
                                              disabled_features);
    }

    scoped_feature_list_.InitWithFeatureList(std::move(feature_list));
  }

  ~ScopedUkmFeatureParams() { variations::testing::ClearAllVariationParams(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ScopedUkmFeatureParams);
};

class UkmServiceTest : public testing::Test {
 public:
  UkmServiceTest()
      : task_runner_(new base::TestSimpleTaskRunner),
        task_runner_handle_(task_runner_) {
    UkmService::RegisterPrefs(prefs_.registry());
    ClearPrefs();
  }

  void ClearPrefs() {
    prefs_.ClearPref(prefs::kUkmClientId);
    prefs_.ClearPref(prefs::kUkmSessionId);
    prefs_.ClearPref(prefs::kUkmPersistedLogs);
  }

  int GetPersistedLogCount() {
    const base::ListValue* list_value =
        prefs_.GetList(prefs::kUkmPersistedLogs);
    return list_value->GetSize();
  }

  Report GetPersistedReport() {
    EXPECT_GE(GetPersistedLogCount(), 1);
    metrics::PersistedLogs result_persisted_logs(
        base::MakeUnique<ukm::PersistedLogsMetricsImpl>(), &prefs_,
        prefs::kUkmPersistedLogs,
        3,     // log count limit
        1000,  // byte limit
        0);

    result_persisted_logs.LoadPersistedUnsentLogs();
    result_persisted_logs.StageNextLog();

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
  service.EnableRecording();
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
  service.EnableRecording();
  service.EnableReporting();

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Should init, generate a log, and start an upload for source.
  task_runner_->RunPendingTasks();
  EXPECT_TRUE(client_.uploader()->is_uploading());
  // Flushes the generated log to disk and generates a new entry.
  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
    builder->AddMetric("FirstContentfulPaint", 300);
  }
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
  service.EnableRecording();
  service.EnableReporting();

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/initial"));
  service.UpdateSourceURL(id, GURL("https://google.com/intermediate"));
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  EXPECT_FALSE(proto_report.has_session_id());
  const Source& proto_source = proto_report.sources(0);

  EXPECT_EQ(id, proto_source.id());
  EXPECT_EQ(GURL("https://google.com/foobar").spec(), proto_source.url());
  EXPECT_FALSE(proto_source.has_initial_url());
}

TEST_F(UkmServiceTest, EntryBuilderAndSerialization) {
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording();
  service.EnableReporting();

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  {
    std::unique_ptr<UkmEntryBuilder> foo_builder =
        service.GetEntryBuilder(id, "foo");
    foo_builder->AddMetric("foo_start", 0);
    foo_builder->AddMetric("foo_end", 10);

    std::unique_ptr<UkmEntryBuilder> bar_builder =
        service.GetEntryBuilder(id, "bar");
    bar_builder->AddMetric("bar_start", 5);
    bar_builder->AddMetric("bar_end", 15);
  }

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  Report proto_report = GetPersistedReport();

  EXPECT_EQ(1, proto_report.sources_size());
  const Source& proto_source = proto_report.sources(0);
  EXPECT_EQ(GURL("https://google.com/foobar").spec(), proto_source.url());
  EXPECT_EQ(id, proto_source.id());

  EXPECT_EQ(2, proto_report.entries_size());

  // Bar entry is the 0th entry here because bar_builder is destructed before
  // foo_builder: the reverse order as they are constructed. To have the same
  // ordering as the builders are constructed, one can achieve that by putting
  // builders in separate scopes.
  const Entry& proto_entry_bar = proto_report.entries(0);
  EXPECT_EQ(id, proto_entry_bar.source_id());
  EXPECT_EQ(base::HashMetricName("bar"), proto_entry_bar.event_hash());
  EXPECT_EQ(2, proto_entry_bar.metrics_size());
  const Entry::Metric proto_entry_bar_start = proto_entry_bar.metrics(0);
  EXPECT_EQ(base::HashMetricName("bar_start"),
            proto_entry_bar_start.metric_hash());
  EXPECT_EQ(5, proto_entry_bar_start.value());
  const Entry::Metric proto_entry_bar_end = proto_entry_bar.metrics(1);
  EXPECT_EQ(base::HashMetricName("bar_end"), proto_entry_bar_end.metric_hash());
  EXPECT_EQ(15, proto_entry_bar_end.value());

  const Entry& proto_entry_foo = proto_report.entries(1);
  EXPECT_EQ(id, proto_entry_foo.source_id());
  EXPECT_EQ(base::HashMetricName("foo"), proto_entry_foo.event_hash());
  EXPECT_EQ(2, proto_entry_foo.metrics_size());
  const Entry::Metric proto_entry_foo_start = proto_entry_foo.metrics(0);
  EXPECT_EQ(base::HashMetricName("foo_start"),
            proto_entry_foo_start.metric_hash());
  EXPECT_EQ(0, proto_entry_foo_start.value());
  const Entry::Metric proto_entry_foo_end = proto_entry_foo.metrics(1);
  EXPECT_EQ(base::HashMetricName("foo_end"), proto_entry_foo_end.metric_hash());
  EXPECT_EQ(10, proto_entry_foo_end.value());
}

TEST_F(UkmServiceTest, AddEntryOnlyWithNonEmptyMetrics) {
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording();
  service.EnableReporting();

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));

  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
  }
  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());
  Report proto_report = GetPersistedReport();
  EXPECT_EQ(0, proto_report.entries_size());

  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
    builder->AddMetric("FirstContentfulPaint", 300);
  }
  service.Flush();
  EXPECT_EQ(2, GetPersistedLogCount());
  Report proto_report_with_entries = GetPersistedReport();
  EXPECT_EQ(1, proto_report_with_entries.entries_size());
}

TEST_F(UkmServiceTest, MetricsProviderTest) {
  UkmService service(&prefs_, &client_);

  metrics::TestMetricsProvider* provider = new metrics::TestMetricsProvider();
  service.RegisterMetricsProvider(
      std::unique_ptr<metrics::MetricsProvider>(provider));

  service.Initialize();

  // Providers have not supplied system profile information yet.
  EXPECT_FALSE(provider->provide_system_profile_metrics_called());

  task_runner_->RunUntilIdle();
  service.EnableRecording();
  service.EnableReporting();

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
    builder->AddMetric("FirstContentfulPaint", 300);
  }
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  Report proto_report = GetPersistedReport();
  EXPECT_EQ(1, proto_report.sources_size());
  EXPECT_EQ(1, proto_report.entries_size());

  // Providers have now supplied system profile information.
  EXPECT_TRUE(provider->provide_system_profile_metrics_called());
}

TEST_F(UkmServiceTest, LogsUploadedOnlyWhenHavingSourcesOrEntries) {
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(GetPersistedLogCount(), 0);
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording();
  service.EnableReporting();

  EXPECT_TRUE(task_runner_->HasPendingTask());
  // Neither rotation or Flush should generate logs
  task_runner_->RunPendingTasks();
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 0);

  int32_t id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  // Includes a Source, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 1);

  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
    builder->AddMetric("FirstPaint", 300);
  }
  // Includes an Entry, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 2);

  service.UpdateSourceURL(id, GURL("https://google.com/foobar"));
  {
    std::unique_ptr<UkmEntryBuilder> builder =
        service.GetEntryBuilder(id, "PageLoad");
    builder->AddMetric("FirstContentfulPaint", 300);
  }
  // Includes a Source and an Entry, so will persist.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 3);

  // Current log has no Sources.
  service.Flush();
  EXPECT_EQ(GetPersistedLogCount(), 3);
}

TEST_F(UkmServiceTest, GetNewSourceID) {
  int32_t id1 = UkmService::GetNewSourceID();
  int32_t id2 = UkmService::GetNewSourceID();
  int32_t id3 = UkmService::GetNewSourceID();
  EXPECT_NE(id1, id2);
  EXPECT_NE(id1, id3);
  EXPECT_NE(id2, id3);
}

TEST_F(UkmServiceTest, RecordInitialUrl) {
  for (bool should_record_initial_url : {true, false}) {
    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    ScopedUkmFeatureParams params(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        {{"RecordInitialUrl", should_record_initial_url ? "true" : "false"}});

    ClearPrefs();
    UkmService service(&prefs_, &client_);
    EXPECT_EQ(GetPersistedLogCount(), 0);
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording();
    service.EnableReporting();

    int32_t id = UkmService::GetNewSourceID();
    service.UpdateSourceURL(id, GURL("https://google.com/initial"));
    service.UpdateSourceURL(id, GURL("https://google.com/intermediate"));
    service.UpdateSourceURL(id, GURL("https://google.com/foobar"));

    service.Flush();
    EXPECT_EQ(GetPersistedLogCount(), 1);

    Report proto_report = GetPersistedReport();
    EXPECT_EQ(1, proto_report.sources_size());
    const Source& proto_source = proto_report.sources(0);

    EXPECT_EQ(id, proto_source.id());
    EXPECT_EQ(GURL("https://google.com/foobar").spec(), proto_source.url());
    EXPECT_EQ(should_record_initial_url, proto_source.has_initial_url());
    if (should_record_initial_url) {
      EXPECT_EQ(GURL("https://google.com/initial").spec(),
                proto_source.initial_url());
    }
  }
}

TEST_F(UkmServiceTest, RecordSessionId) {
  for (bool should_record_session_id : {true, false}) {
    base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
    ScopedUkmFeatureParams params(
        base::FeatureList::OVERRIDE_ENABLE_FEATURE,
        {{"RecordSessionId", should_record_session_id ? "true" : "false"}});

    ClearPrefs();
    UkmService service(&prefs_, &client_);
    EXPECT_EQ(0, GetPersistedLogCount());
    service.Initialize();
    task_runner_->RunUntilIdle();
    service.EnableRecording();
    service.EnableReporting();

    auto id = UkmService::GetNewSourceID();
    service.UpdateSourceURL(id, GURL("https://google.com/foobar"));

    service.Flush();
    EXPECT_EQ(1, GetPersistedLogCount());

    auto proto_report = GetPersistedReport();
    EXPECT_EQ(should_record_session_id, proto_report.has_session_id());
  }
}

TEST_F(UkmServiceTest, SourceSize) {
  base::FieldTrialList field_trial_list(nullptr /* entropy_provider */);
  // Set a threshold of number of Sources via Feature Params.
  ScopedUkmFeatureParams params(base::FeatureList::OVERRIDE_ENABLE_FEATURE,
                                {{"MaxSources", "2"}});

  ClearPrefs();
  UkmService service(&prefs_, &client_);
  EXPECT_EQ(0, GetPersistedLogCount());
  service.Initialize();
  task_runner_->RunUntilIdle();
  service.EnableRecording();
  service.EnableReporting();

  auto id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar1"));
  id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar2"));
  id = UkmService::GetNewSourceID();
  service.UpdateSourceURL(id, GURL("https://google.com/foobar3"));

  service.Flush();
  EXPECT_EQ(1, GetPersistedLogCount());

  auto proto_report = GetPersistedReport();
  // Note, 2 instead of 3 sources, since we overrode the max number of sources
  // via Feature params.
  EXPECT_EQ(2, proto_report.sources_size());
}

}  // namespace ukm
