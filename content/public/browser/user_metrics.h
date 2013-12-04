// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USER_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_USER_METRICS_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"
#include "content/public/common/user_metrics_action.h"

namespace content {

// This module provides some helper functions for logging actions tracked by
// the user metrics system.

// Record that the user performed an action.
// "Action" here means a user-generated event:
//   good: "Reload", "CloseTab", and "IMEInvoked"
//   not good: "SSLDialogShown", "PageLoaded", "DiskFull"
// We use this to gather anonymized information about how users are
// interacting with the browser.
// WARNING: In calls to this function, UserMetricsAction and a
// string literal parameter must be on the same line, e.g.
//   content::RecordAction(
//       content::UserMetricsAction("my extremely long action name"));
// This ensures that our processing scripts can associate this action's hash
// with its metric name. Therefore, it will be possible to retrieve the metric
// name from the hash later on.
//
// Once a new recorded action is added, run
// tools/metrics/actions/extract_actions.py --hash
// to generate a new mapping of [action hashes -> metric names] and send it
// out for review to be updated.
//
// For more complicated situations (like when there are many different
// possible actions), see RecordComputedAction.
CONTENT_EXPORT void RecordAction(const UserMetricsAction& action);

// This function has identical input and behavior to RecordAction, but is
// not automatically found by the action-processing scripts.  It can be used
// when it's a pain to enumerate all possible actions, but if you use this
// you need to also update the rules for extracting known actions in
// tools/metrics/actions/extract_actions.py.
CONTENT_EXPORT void RecordComputedAction(const std::string& action);

// Called with the action string.
typedef base::Callback<void(const std::string&)> ActionCallback;

// Add/remove action callbacks (see above).
CONTENT_EXPORT void AddActionCallback(const ActionCallback& callback);
CONTENT_EXPORT void RemoveActionCallback(const ActionCallback& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USER_METRICS_H_
