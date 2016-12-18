// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TODAY_EXTENSION_TODAY_METRICS_LOGGER_H_
#define IOS_CHROME_TODAY_EXTENSION_TODAY_METRICS_LOGGER_H_

#include <memory>
#import "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram_flattener.h"
#include "base/metrics/histogram_snapshot_manager.h"
#include "base/metrics/user_metrics_action.h"

namespace base {

class SequencedWorkerPool;
class SequencedTaskRunner;

}  // namespace base

namespace {

class TodayMetricsLog;
class TodayMetricsServiceClient;

}  // namespace

class ValueMapPrefStore;
class PrefRegistrySimple;
class PrefService;

// Utility class to create metrics log that can be pushed to Chrome. The
// extension creates and fills the logs with UserAction. The upload is done by
// the Chrome application.
class TodayMetricsLogger : base::HistogramFlattener {
 public:
  // Singleton.
  static TodayMetricsLogger* GetInstance();

  // Records a user action. The log is saved in the user defaults after each
  // action.
  void RecordUserAction(base::UserMetricsAction action);

  // Write the current log in the UserDefaults.
  void PersistLogs();

  // HistogramFlattener:
  void RecordDelta(const base::HistogramBase& histogram,
                   const base::HistogramSamples& snapshot) override;
  void InconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void UniqueInconsistencyDetected(
      base::HistogramBase::Inconsistency problem) override;
  void InconsistencyDetectedInLoggedCount(int amount) override;

 private:
  TodayMetricsLogger();
  ~TodayMetricsLogger() override;

  bool CreateNewLog();

  base::MessageLoop message_loop_;
  scoped_refptr<PrefRegistrySimple> pref_registry_;
  std::unique_ptr<PrefService> pref_service_;
  scoped_refptr<ValueMapPrefStore> value_map_prefs_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  std::unique_ptr<TodayMetricsLog> log_;
  scoped_refptr<base::SequencedWorkerPool> thread_pool_;
  std::unique_ptr<TodayMetricsServiceClient> metrics_service_client_;
  base::HistogramSnapshotManager histogram_snapshot_manager_;

  DISALLOW_COPY_AND_ASSIGN(TodayMetricsLogger);
};

#endif  // IOS_CHROME_TODAY_EXTENSION_TODAY_METRICS_LOGGER_H_
