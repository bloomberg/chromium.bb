// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_log.h"

namespace net {

LoadLog::LoadLog() {
}

// static
const char* LoadLog::EventTypeToString(EventType event) {
  switch (event) {
#define EVENT_TYPE(label) case TYPE_ ## label: return #label;
#include "net/base/load_log_event_type_list.h"
#undef EVENT_TYPE
  }
  return NULL;
}

void LoadLog::Add(base::TimeTicks t, EventType event, EventPhase phase) {
  // Minor optimization. TODO(eroman): use StackVector instead.
  if (events_.empty())
    events_.reserve(kMaxNumEntries / 2);

  // Enforce a bound of kMaxNumEntries -- when we reach it, make it so the
  // final entry in the list is |TYPE_LOG_TRUNCATED|.

  if (events_.size() + 1 == kMaxNumEntries)
    events_.push_back(Event(t, TYPE_LOG_TRUNCATED, PHASE_NONE));

  if (events_.size() < kMaxNumEntries)
    events_.push_back(Event(t, event, phase));
}

}  // namespace net
