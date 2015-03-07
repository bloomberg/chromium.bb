// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_USER_METRICS_H_
#define IOS_WEB_PUBLIC_USER_METRICS_H_

#include <string>

#include "base/callback.h"

namespace base {
struct UserMetricsAction;
}  // namespace base

namespace web {

// Wrappers around functions defined in base/metrics/user_metrics.h, refer to
// that header for full documentation. These wrappers can be called from any
// thread (they will post back to the UI thread to do the recording).

void RecordAction(const base::UserMetricsAction& action);
void RecordComputedAction(const std::string& action);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_USER_METRICS_H_
