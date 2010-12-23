// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_USER_METRICS_H_
#define CHROME_BROWSER_METRICS_USER_METRICS_H_
#pragma once

#include <string>

class Profile;

// This module provides some helper functions for logging actions tracked by
// the user metrics system.


// UserMetricsAction exist purely to standardize on the paramters passed to
// UserMetrics. That way, our toolset can scan the sourcecode reliable for
// constructors and extract the associated string constants
struct UserMetricsAction {
  const char* str_;
  explicit UserMetricsAction(const char* str) : str_(str) {}
};


class UserMetrics {
 public:
  // Record that the user performed an action.
  // "Action" here means a user-generated event:
  //   good: "Reload", "CloseTab", and "IMEInvoked"
  //   not good: "SSLDialogShown", "PageLoaded", "DiskFull"
  // We use this to gather anonymized information about how users are
  // interacting with the browser.
  // WARNING: Call this function exactly like this:
  //   UserMetrics::RecordAction(UserMetricsAction("foo bar"));
  // (all on one line and with the metric string literal [no variables])
  // because otherwise our processing scripts won't pick up on new actions.
  //
  // Once a new recorded action is added, run chrome/tools/extract_actions.py
  // to generate a new mapping of [action hashes -> metric names] and send it
  // out for review to be updated.
  //
  // For more complicated situations (like when there are many different
  // possible actions), see RecordComputedAction.
  //
  // TODO(semenzato): |profile| isn't actually used---should switch all calls
  // to the version without it.
  static void RecordAction(const UserMetricsAction& action, Profile* profile);

  // This function has identical input and behavior to RecordAction, but is
  // not automatically found by the action-processing scripts.  It can be used
  // when it's a pain to enumerate all possible actions, but if you use this
  // you need to also update the rules for extracting known actions in
  // chrome/tools/extract_actions.py.
  static void RecordComputedAction(const std::string& action,
                                   Profile* profile);

  static void RecordAction(const UserMetricsAction& action);
  static void RecordComputedAction(const std::string& action);

 private:
  static void Record(const char *action, Profile *profile);
  static void Record(const char *action);
  static void CallRecordOnUI(const std::string& action);
};

#endif  // CHROME_BROWSER_METRICS_USER_METRICS_H_
