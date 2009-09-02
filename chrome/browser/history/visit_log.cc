// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/visit_log.h"

namespace history {

static VisitLog* g_visit_log = NULL;

VisitLog::VisitLog()
    : index_(0),
      num_add_(0),
      num_delete_(0),
      num_update_(0),
      num_select_(0) {
  memset(events_, NO_OP, sizeof(events_));
}

VisitLog::~VisitLog() {
}

void VisitLog::AddEvent(EventType event) {
  switch (event) {
    case ADD_VISIT:
      num_add_++;
      break;
    case DELETE_VISIT:
      num_delete_++;
      break;
    case UPDATE_VISIT:
      num_update_++;
      break;
    case SELECT_VISIT:
      num_select_++;
      break;
    default:
      return;
  }

  events_[index_++] = event;
  if (index_ == kVisitLogBufferSize)
    index_ = 0;
}

void InitVisitLog(VisitLog* vlog) {
  g_visit_log = vlog;
}

void AddEventToVisitLog(VisitLog::EventType event) {
  if (!g_visit_log)
    return;
  g_visit_log->AddEvent(event);
}

}  // namespace history
