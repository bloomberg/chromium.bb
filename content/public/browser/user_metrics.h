// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_USER_METRICS_H_
#define CONTENT_PUBLIC_BROWSER_USER_METRICS_H_

#include <string>

#include "base/callback.h"
#include "content/common/content_export.h"

namespace content {

// This module provides some helper functions for logging actions tracked by
// the user metrics system.


// UserMetricsAction exist purely to standardize on the paramters passed to
// UserMetrics. That way, our toolset can scan the sourcecode reliable for
// constructors and extract the associated string constants
struct UserMetricsAction {
  const char* str_;
  explicit UserMetricsAction(const char* str) : str_(str) {}
};

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
// because otherwise our processing scripts won't pick up on new actions.
//
// Once a new recorded action is added, run chrome/tools/extract_actions.py
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
// chrome/tools/extract_actions.py.
CONTENT_EXPORT void RecordComputedAction(const std::string& action);

// Called with the action string.
typedef base::Callback<void(const std::string&)> ActionCallback;

// Add/remove action callbacks (see above).
CONTENT_EXPORT void AddActionCallback(const ActionCallback& callback);
CONTENT_EXPORT void RemoveActionCallback(const ActionCallback& callback);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_USER_METRICS_H_
