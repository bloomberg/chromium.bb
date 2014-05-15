// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_log_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "components/metrics/metrics_log_base.h"
#include "components/metrics/metrics_pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

// Dummy serializer that just stores logs in memory.
class TestLogPrefService : public TestingPrefServiceSimple {
 public:
  TestLogPrefService() {
    registry()->RegisterListPref(prefs::kMetricsInitialLogs);
    registry()->RegisterListPref(prefs::kMetricsOngoingLogs);
  }
  // Returns the number of logs of the given type.
  size_t TypeCount(MetricsLogManager::LogType log_type) {
    int list_length = 0;
    if (log_type == MetricsLogBase::INITIAL_STABILITY_LOG)
      list_length = GetList(prefs::kMetricsInitialLogs)->GetSize();
    else
      list_length = GetList(prefs::kMetricsOngoingLogs)->GetSize();
    return list_length ? list_length - 2 : 0;
  }
};

}  // namespace

TEST(MetricsLogManagerTest, StandardFlow) {
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  // Make sure a new manager has a clean slate.
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.has_unsent_logs());

  // Check that the normal flow works.
  MetricsLogBase* initial_log =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
  log_manager.BeginLoggingWithLog(initial_log);
  EXPECT_EQ(initial_log, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());

  log_manager.FinishCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_TRUE(log_manager.has_unsent_logs());
  EXPECT_FALSE(log_manager.has_staged_log());

  MetricsLogBase* second_log =
      new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
  log_manager.BeginLoggingWithLog(second_log);
  EXPECT_EQ(second_log, log_manager.current_log());

  log_manager.StageNextLogForUpload();
  EXPECT_TRUE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.staged_log().empty());

  log_manager.DiscardStagedLog();
  EXPECT_EQ(second_log, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.has_unsent_logs());

  EXPECT_FALSE(log_manager.has_unsent_logs());
}

TEST(MetricsLogManagerTest, AbandonedLog) {
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  MetricsLogBase* dummy_log =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
  log_manager.BeginLoggingWithLog(dummy_log);
  EXPECT_EQ(dummy_log, log_manager.current_log());

  log_manager.DiscardCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
}

TEST(MetricsLogManagerTest, InterjectedLog) {
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);

  MetricsLogBase* ongoing_log =
      new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
  MetricsLogBase* temp_log =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");

  log_manager.BeginLoggingWithLog(ongoing_log);
  EXPECT_EQ(ongoing_log, log_manager.current_log());

  log_manager.PauseCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());

  log_manager.BeginLoggingWithLog(temp_log);
  EXPECT_EQ(temp_log, log_manager.current_log());
  log_manager.FinishCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());

  log_manager.ResumePausedLog();
  EXPECT_EQ(ongoing_log, log_manager.current_log());

  EXPECT_FALSE(log_manager.has_staged_log());
  log_manager.StageNextLogForUpload();
  log_manager.DiscardStagedLog();
  EXPECT_FALSE(log_manager.has_unsent_logs());
}

TEST(MetricsLogManagerTest, InterjectedLogPreservesType) {
  TestLogPrefService pref_service;
  MetricsLogManager log_manager(&pref_service, 0);
  log_manager.LoadPersistedUnsentLogs();

  MetricsLogBase* ongoing_log =
      new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
  MetricsLogBase* temp_log =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");

  log_manager.BeginLoggingWithLog(ongoing_log);
  log_manager.PauseCurrentLog();
  log_manager.BeginLoggingWithLog(temp_log);
  log_manager.FinishCurrentLog();
  log_manager.ResumePausedLog();
  log_manager.StageNextLogForUpload();
  log_manager.DiscardStagedLog();

  // Verify that the remaining log (which is the original ongoing log) still
  // has the right type.
  log_manager.FinishCurrentLog();
  log_manager.PersistUnsentLogs();
  EXPECT_EQ(0U, pref_service.TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
  EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, StoreAndLoad) {
  TestLogPrefService pref_service;
  // Set up some in-progress logging in a scoped log manager simulating the
  // leadup to quitting, then persist as would be done on quit.
  {
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    // Simulate a log having already been unsent from a previous session.
    {
      std::string log("proto");
      metrics::PersistedLogs ongoing_logs(
          &pref_service, prefs::kMetricsOngoingLogs, 1, 1, 0);
      ongoing_logs.StoreLog(&log);
      ongoing_logs.SerializeLogs();
    }
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
    EXPECT_FALSE(log_manager.has_unsent_logs());
    log_manager.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.BeginLoggingWithLog(log2);
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(metrics::PersistedLogs::NORMAL_STORE);
    log_manager.FinishCurrentLog();

    // Nothing should be written out until PersistUnsentLogs is called.
    EXPECT_EQ(0U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }

  // Now simulate the relaunch, ensure that the log manager restores
  // everything correctly, and verify that once the are handled they are not
  // re-persisted.
  {
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    // The initial log should be sent first; update the persisted storage to
    // verify.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));

    // Handle the first ongoing log.
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    // Handle the last log.
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    EXPECT_FALSE(log_manager.has_unsent_logs());

    // Nothing should have changed "on disk" since PersistUnsentLogs hasn't been
    // called again.
    EXPECT_EQ(2U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
    // Persist, and make sure nothing is left.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, StoreStagedLogTypes) {
  // Ensure that types are preserved when storing staged logs.
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(metrics::PersistedLogs::NORMAL_STORE);
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(0U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }

  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log =
        new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
    log_manager.BeginLoggingWithLog(log);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(metrics::PersistedLogs::NORMAL_STORE);
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(1U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, LargeLogDiscarding) {
  TestLogPrefService pref_service;
  // Set the size threshold very low, to verify that it's honored.
  MetricsLogManager log_manager(&pref_service, 1);
  log_manager.LoadPersistedUnsentLogs();

  MetricsLogBase* log1 =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
  MetricsLogBase* log2 =
      new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
  log_manager.BeginLoggingWithLog(log1);
  log_manager.FinishCurrentLog();
  log_manager.BeginLoggingWithLog(log2);
  log_manager.FinishCurrentLog();

  // Only the ongoing log should be written out, due to the threshold.
  log_manager.PersistUnsentLogs();
  EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, ProvisionalStoreStandardFlow) {
  // Ensure that provisional store works, and discards the correct log.
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.BeginLoggingWithLog(log2);
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(
        metrics::PersistedLogs::PROVISIONAL_STORE);
    log_manager.FinishCurrentLog();
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, pref_service.TypeCount(
        MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, ProvisionalStoreNoop) {
  // Ensure that trying to drop a sent log is a no-op, even if another log has
  // since been staged.
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(
        metrics::PersistedLogs::PROVISIONAL_STORE);
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    log_manager.BeginLoggingWithLog(log2);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(metrics::PersistedLogs::NORMAL_STORE);
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }

  // Ensure that trying to drop more than once is a no-op
  {
    TestLogPrefService pref_service;
    MetricsLogManager log_manager(&pref_service, 0);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(metrics::PersistedLogs::NORMAL_STORE);
    log_manager.BeginLoggingWithLog(log2);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(
        metrics::PersistedLogs::PROVISIONAL_STORE);
    log_manager.DiscardLastProvisionalStore();
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, pref_service.TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

}  // namespace metrics
