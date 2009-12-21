// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/load_log.h"
#include "base/logging.h"

namespace net {

LoadLog::LoadLog(size_t max_num_entries)
    : num_entries_truncated_(0), max_num_entries_(max_num_entries) {
  DCHECK_GT(max_num_entries, 0u);
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

void LoadLog::Add(const Entry& entry) {
  // Minor optimization. TODO(eroman): use StackVector instead.
  if (entries_.empty())
    entries_.reserve(10);  // It is likely we will have at least 10 entries.

  // Enforce a bound of |max_num_entries_| -- once we reach it, keep overwriting
  // the final entry in the log.

  if (entries_.size() + 1 <= max_num_entries_ ||
      max_num_entries_ == kUnbounded) {
    entries_.push_back(entry);
  } else {
    num_entries_truncated_ += 1;
    entries_[max_num_entries_ - 1] = entry;
  }
}

void LoadLog::Append(const LoadLog* log) {
  for (size_t i = 0; i < log->entries().size(); ++i)
    Add(log->entries()[i]);
  num_entries_truncated_ += log->num_entries_truncated();
}

}  // namespace net
