// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_VISIT_LOG_H_
#define CHROME_BROWSER_HISTORY_VISIT_LOG_H_

#include "base/basictypes.h"

namespace history {

// VisitLog provides support for basic in-memory logging of history visit
// events. It keeps a circular buffer where the recent visit events is
// recorded.

// This class is not thread safe.
class VisitLog {
 public:
  VisitLog();
  ~VisitLog();

  enum EventType {
    NO_OP,
    ADD_VISIT,
    DELETE_VISIT,
    UPDATE_VISIT,
    SELECT_VISIT,
  };

  static int events_buffer_size() { return kVisitLogBufferSize; }
  unsigned char* events() { return events_; }
  int index() { return index_; }

  int num_add() { return num_add_; }
  int num_delete() { return num_delete_; }
  int num_update() { return num_update_; }
  int num_select() { return num_select_; }

  void AddEvent(EventType event);

 private:
  // Circular buffer of recent events.
  static const int kVisitLogBufferSize = 4096;
  unsigned char events_[kVisitLogBufferSize];
  int index_;

  // Event counters
  int num_add_;
  int num_delete_;
  int num_update_;
  int num_select_;

  DISALLOW_COPY_AND_ASSIGN(VisitLog);
};

void InitVisitLog(VisitLog* vlog);
void AddEventToVisitLog(VisitLog::EventType event);

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_VISIT_LOG_H_
