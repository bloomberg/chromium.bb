// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/user_metrics.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/metrics/user_metrics.h"
#include "ios/web/public/web_thread.h"

namespace web {

// TODO(beaudoin): Get rid of these methods now that the base:: version does
// thread hopping. Tracked in crbug.com/601483.
void RecordAction(const base::UserMetricsAction& action) {
  base::RecordAction(action);
}

void RecordComputedAction(const std::string& action) {
  base::RecordComputedAction(action);
}

}  // namespace web
