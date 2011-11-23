// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_HISTORY_FIELD_TRIAL_H_
#define CHROME_BROWSER_HISTORY_HISTORY_FIELD_TRIAL_H_
#pragma once

#include <string>

namespace history {

class HistoryFieldTrial {
 public:
  static void Activate();

  // Returns true if history should try to use less memory.
  static bool IsLowMemFieldTrial();

  // Returns the group name as a suffix to append to histogram names for the
  // current field trial, or the empty string if none.
  static std::string GetGroupSuffix();
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_HISTORY_FIELD_TRIAL_H_
