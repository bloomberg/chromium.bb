// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTED_LOGS_METRICS_H_
#define COMPONENTS_METRICS_PERSISTED_LOGS_METRICS_H_

#include "base/macros.h"
#include "components/metrics/persisted_logs.h"

namespace metrics {

// Interface for recording metrics from PersistedLogs.
class PersistedLogsMetrics {
 public:
  PersistedLogsMetrics() {}
  virtual ~PersistedLogsMetrics() {}

  virtual PersistedLogs::LogReadStatus RecordLogReadStatus(
      PersistedLogs::LogReadStatus status) = 0;

  virtual void RecordCompressionRatio(
    size_t compressed_size, size_t original_size) {}

  virtual void RecordDroppedLogSize(size_t size) {}

  virtual void RecordDroppedLogsNum(int dropped_logs_num) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PersistedLogsMetrics);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PERSISTED_LOGS_METRICS_H_
