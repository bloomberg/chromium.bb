// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/base/legacymetrics_user_event_recorder.h"

#include <utility>

#include "base/callback.h"
#include "base/metrics/user_metrics.h"

namespace cr_fuchsia {

LegacyMetricsUserActionRecorder::LegacyMetricsUserActionRecorder()
    : on_event_callback_(
          base::BindRepeating(&LegacyMetricsUserActionRecorder::OnUserAction,
                              base::Unretained(this))) {
  base::AddActionCallback(on_event_callback_);
}

LegacyMetricsUserActionRecorder::~LegacyMetricsUserActionRecorder() {
  base::RemoveActionCallback(on_event_callback_);
}

bool LegacyMetricsUserActionRecorder::HasEvents() const {
  return !events_.empty();
}

std::vector<fuchsia::legacymetrics::UserActionEvent>
LegacyMetricsUserActionRecorder::TakeEvents() {
  return std::move(events_);
}

void LegacyMetricsUserActionRecorder::OnUserAction(const std::string& action,
                                                   base::TimeTicks time) {
  fuchsia::legacymetrics::UserActionEvent fidl_event;
  fidl_event.set_name(action);
  fidl_event.set_time(time.ToZxTime());
  events_.push_back(std::move(fidl_event));
}

}  // namespace cr_fuchsia
