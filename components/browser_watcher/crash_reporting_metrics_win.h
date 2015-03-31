// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_WATCHER_CRASH_REPORTING_METRICS_WIN_H_
#define COMPONENTS_BROWSER_WATCHER_CRASH_REPORTING_METRICS_WIN_H_

#include "base/strings/string16.h"

namespace browser_watcher {

// Stores and retrieves metrics related to crash dump attempts.
class CrashReportingMetrics {
 public:
  // Represents the currently stored metrics.
  struct Values {
    // A count of crash dump attempts.
    int crash_dump_attempts;
    // A count of successful crash dump attempts.
    int successful_crash_dumps;
    // A count of failed crash dump attempts.
    int failed_crash_dumps;
    // A count of dump without crash attempts.
    int dump_without_crash_attempts;
    // A count of successful dump without crash attempts.
    int successful_dumps_without_crash;
    // A count of failed dump without crash attempts.
    int failed_dumps_without_crash;
  };

  // Instantiates an instance that will store and retrieve its metrics from
  // |registry_path|.
  explicit CrashReportingMetrics(const base::string16& registry_path);

  // Records that a crash dump is being attempted.
  void RecordCrashDumpAttempt();

  // Records that a dump without crash is being attempted.
  void RecordDumpWithoutCrashAttempt();

  // Records the result of a crash dump attempt.
  void RecordCrashDumpAttemptResult(bool succeeded);

  // Records the result of a dump without crash attempt.
  void RecordDumpWithoutCrashAttemptResult(bool succeeded);

  // Returns the currently stored metrics and resets them to 0.
  Values RetrieveAndResetMetrics();

 private:
  base::string16 registry_path_;
};

}  // namespace browser_watcher

#endif  // COMPONENTS_BROWSER_WATCHER_CRASH_REPORTING_METRICS_WIN_H_
