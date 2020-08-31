// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_BASE_LEGACYMETRICS_USER_EVENT_RECORDER_H_
#define FUCHSIA_BASE_LEGACYMETRICS_USER_EVENT_RECORDER_H_

#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <string>
#include <vector>

#include "base/metrics/user_metrics.h"

namespace cr_fuchsia {

// Captures and stores user action events, and converts them to
// fuchsia.legacymetrics equivalent.
class LegacyMetricsUserActionRecorder {
 public:
  LegacyMetricsUserActionRecorder();
  ~LegacyMetricsUserActionRecorder();

  LegacyMetricsUserActionRecorder(const LegacyMetricsUserActionRecorder&) =
      delete;
  LegacyMetricsUserActionRecorder& operator=(
      const LegacyMetricsUserActionRecorder&) = delete;

  bool HasEvents() const;
  std::vector<fuchsia::legacymetrics::UserActionEvent> TakeEvents();

 private:
  // base::ActionCallback implementation.
  void OnUserAction(const std::string& action, base::TimeTicks time);

  std::vector<fuchsia::legacymetrics::UserActionEvent> events_;
  const base::ActionCallback on_event_callback_;
};

}  // namespace cr_fuchsia

#endif  // FUCHSIA_BASE_LEGACYMETRICS_USER_EVENT_RECORDER_H_
