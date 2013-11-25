// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_events.h"

#include "base/basictypes.h"
#include "base/logging.h"

namespace prerender {

namespace {

const char* kEventNames[] = {
  "swapin no delegate",
  "swapin candidate",
  "swapin candidate namespace matches",
  "swapin no merge pending",
  "swapin merging disabled",
  "swapin issuing merge",
  "swapin merge for swapin candidate",
  "merge result no pending swapin",
  "merge result timeout cb",
  "merge result result cb",
  "merge result timed out",
  "merge result merge done",
  "merge result result namespace not found",
  "merge result result namespace not alias",
  "merge result result not logging",
  "merge result result no transactions",
  "merge result result too many transactions",
  "merge result result not mergeable",
  "merge result result mergeable",
  "merge result merge failed",
  "merge result swapping in",
  "merge result swapin successful",
  "merge result swapin failed",
  "Max",
};

COMPILE_ASSERT(arraysize(kEventNames) == PRERENDER_EVENT_MAX + 1,
               PrerenderEvent_name_count_mismatch);

}  // namespace

const char* NameFromPrerenderEvent(PrerenderEvent event) {
  DCHECK(static_cast<int>(event) >= 0 &&
         event <= PRERENDER_EVENT_MAX);
  return kEventNames[event];
}

}  // namespace prerender
