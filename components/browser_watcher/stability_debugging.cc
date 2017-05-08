// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_watcher/stability_debugging.h"

#include "base/debug/activity_tracker.h"

namespace browser_watcher {

void SetStabilityDataBool(base::StringPiece name, bool value) {
  base::debug::GlobalActivityTracker* global_tracker =
      base::debug::GlobalActivityTracker::Get();
  if (!global_tracker)
    return;  // Activity tracking isn't enabled.

  global_tracker->process_data().SetBool(name, value);
}

void SetStabilityDataInt(base::StringPiece name, int64_t value) {
  base::debug::GlobalActivityTracker* global_tracker =
      base::debug::GlobalActivityTracker::Get();
  if (!global_tracker)
    return;  // Activity tracking isn't enabled.

  global_tracker->process_data().SetInt(name, value);
}

}  // namespace browser_watcher
