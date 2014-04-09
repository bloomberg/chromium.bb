// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/metrics_log_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/sha1.h"
#include "chrome/common/metrics/metrics_log_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Dummy serializer that just stores logs in memory.
class DummyLogSerializer : public MetricsLogManager::LogSerializer {
 public:
  virtual void SerializeLogs(
      const std::vector<MetricsLogManager::SerializedLog>& logs,
      MetricsLogManager::LogType log_type) OVERRIDE {
    persisted_logs_[log_type] = logs;
  }

  virtual void DeserializeLogs(
      MetricsLogManager::LogType log_type,
      std::vector<MetricsLogManager::SerializedLog>* logs) OVERRIDE {
    ASSERT_NE(static_cast<void*>(NULL), logs);
    *logs = persisted_logs_[log_type];
  }

  // Returns the number of logs of the given type.
  size_t TypeCount(MetricsLogManager::LogType log_type) {
    return persisted_logs_[log_type].size();
  }

  // In-memory "persitent storage".
  std::vector<MetricsLogManager::SerializedLog> persisted_logs_[2];
};

}  // namespace

TEST(MetricsLogManagerTest, StandardFlow) {
  MetricsLogManager log_manager;

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
  EXPECT_FALSE(log_manager.staged_log_text().empty());

  log_manager.DiscardStagedLog();
  EXPECT_EQ(second_log, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
  EXPECT_FALSE(log_manager.has_unsent_logs());
  EXPECT_TRUE(log_manager.staged_log_text().empty());

  EXPECT_FALSE(log_manager.has_unsent_logs());
}

TEST(MetricsLogManagerTest, AbandonedLog) {
  MetricsLogManager log_manager;

  MetricsLogBase* dummy_log =
      new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
  log_manager.BeginLoggingWithLog(dummy_log);
  EXPECT_EQ(dummy_log, log_manager.current_log());

  log_manager.DiscardCurrentLog();
  EXPECT_EQ(NULL, log_manager.current_log());
  EXPECT_FALSE(log_manager.has_staged_log());
}

TEST(MetricsLogManagerTest, InterjectedLog) {
  MetricsLogManager log_manager;

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
  MetricsLogManager log_manager;
  DummyLogSerializer* serializer = new DummyLogSerializer;
  log_manager.set_log_serializer(serializer);
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
  EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
  EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, StoreAndLoad) {
  std::vector<MetricsLogManager::SerializedLog> initial_logs;
  std::vector<MetricsLogManager::SerializedLog> ongoing_logs;

  // Set up some in-progress logging in a scoped log manager simulating the
  // leadup to quitting, then persist as would be done on quit.
  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    // Simulate a log having already been unsent from a previous session.
    MetricsLogManager::SerializedLog log;
    std::string text = "proto";
    log.SwapLogText(&text);
    serializer->persisted_logs_[MetricsLogBase::ONGOING_LOG].push_back(log);
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
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::NORMAL_STORE);
    log_manager.FinishCurrentLog();

    // Nothing should be written out until PersistUnsentLogs is called.
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));

    // Save the logs to transfer over to a new serializer (since log_manager
    // owns |serializer|, so it's about to go away.
    initial_logs =
        serializer->persisted_logs_[MetricsLogBase::INITIAL_STABILITY_LOG];
    ongoing_logs = serializer->persisted_logs_[MetricsLogBase::ONGOING_LOG];
  }

  // Now simulate the relaunch, ensure that the log manager restores
  // everything correctly, and verify that once the are handled they are not
  // re-persisted.
  {
    MetricsLogManager log_manager;

    DummyLogSerializer* serializer = new DummyLogSerializer;
    serializer->persisted_logs_[MetricsLogBase::INITIAL_STABILITY_LOG] =
        initial_logs;
    serializer->persisted_logs_[MetricsLogBase::ONGOING_LOG] = ongoing_logs;

    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();
    EXPECT_TRUE(log_manager.has_unsent_logs());

    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    // The initial log should be sent first; update the persisted storage to
    // verify.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(2U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));

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
    EXPECT_EQ(2U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
    // Persist, and make sure nothing is left.
    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, StoreStagedLogTypes) {
  // Ensure that types are preserved when storing staged logs.
  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::NORMAL_STORE);
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }

  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log =
        new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
    log_manager.BeginLoggingWithLog(log);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::NORMAL_STORE);
    log_manager.PersistUnsentLogs();

    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, LargeLogDiscarding) {
  MetricsLogManager log_manager;
  DummyLogSerializer* serializer = new DummyLogSerializer;
  log_manager.set_log_serializer(serializer);
  log_manager.LoadPersistedUnsentLogs();

  // Set the size threshold very low, to verify that it's honored.
  log_manager.set_max_ongoing_log_store_size(1);

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
  EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
  EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
}

TEST(MetricsLogManagerTest, ProvisionalStoreStandardFlow) {
  // Ensure that provisional store works, and discards the correct log.
  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::INITIAL_STABILITY_LOG, "v");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "v");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.BeginLoggingWithLog(log2);
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::PROVISIONAL_STORE);
    log_manager.FinishCurrentLog();
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(0U, serializer->TypeCount(MetricsLogBase::INITIAL_STABILITY_LOG));
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, ProvisionalStoreNoop) {
  // Ensure that trying to drop a sent log is a no-op, even if another log has
  // since been staged.
  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::PROVISIONAL_STORE);
    log_manager.StageNextLogForUpload();
    log_manager.DiscardStagedLog();
    log_manager.BeginLoggingWithLog(log2);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::NORMAL_STORE);
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }

  // Ensure that trying to drop more than once is a no-op
  {
    MetricsLogManager log_manager;
    DummyLogSerializer* serializer = new DummyLogSerializer;
    log_manager.set_log_serializer(serializer);
    log_manager.LoadPersistedUnsentLogs();

    MetricsLogBase* log1 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    MetricsLogBase* log2 =
        new MetricsLogBase("id", 0, MetricsLogBase::ONGOING_LOG, "version");
    log_manager.BeginLoggingWithLog(log1);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::NORMAL_STORE);
    log_manager.BeginLoggingWithLog(log2);
    log_manager.FinishCurrentLog();
    log_manager.StageNextLogForUpload();
    log_manager.StoreStagedLogAsUnsent(MetricsLogManager::PROVISIONAL_STORE);
    log_manager.DiscardLastProvisionalStore();
    log_manager.DiscardLastProvisionalStore();

    log_manager.PersistUnsentLogs();
    EXPECT_EQ(1U, serializer->TypeCount(MetricsLogBase::ONGOING_LOG));
  }
}

TEST(MetricsLogManagerTest, SerializedLog) {
  const char kFooText[] = "foo";
  const std::string foo_hash = base::SHA1HashString(kFooText);
  const char kBarText[] = "bar";
  const std::string bar_hash = base::SHA1HashString(kBarText);

  MetricsLogManager::SerializedLog log;
  EXPECT_TRUE(log.log_text().empty());
  EXPECT_TRUE(log.log_hash().empty());

  std::string foo = kFooText;
  log.SwapLogText(&foo);
  EXPECT_TRUE(foo.empty());
  EXPECT_FALSE(log.IsEmpty());
  EXPECT_EQ(kFooText, log.log_text());
  EXPECT_EQ(foo_hash, log.log_hash());

  std::string bar = kBarText;
  log.SwapLogText(&bar);
  EXPECT_EQ(kFooText, bar);
  EXPECT_FALSE(log.IsEmpty());
  EXPECT_EQ(kBarText, log.log_text());
  EXPECT_EQ(bar_hash, log.log_hash());

  log.Clear();
  EXPECT_TRUE(log.IsEmpty());
  EXPECT_TRUE(log.log_text().empty());
  EXPECT_TRUE(log.log_hash().empty());

  MetricsLogManager::SerializedLog log2;
  foo = kFooText;
  log2.SwapLogText(&foo);
  log.Swap(&log2);
  EXPECT_FALSE(log.IsEmpty());
  EXPECT_EQ(kFooText, log.log_text());
  EXPECT_EQ(foo_hash, log.log_hash());
  EXPECT_TRUE(log2.IsEmpty());
  EXPECT_TRUE(log2.log_text().empty());
  EXPECT_TRUE(log2.log_hash().empty());
}
