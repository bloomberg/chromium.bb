// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_UTIL_H_
#define NET_BASE_LOAD_LOG_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "net/base/load_log.h"

namespace net {

// The LoadLogUtil utility class contains methods to analyze and visualize
// LoadLogs.

class LoadLogUtil {
 public:
  struct EventDuration {
    LoadLog::EventType event;
    base::TimeDelta duration;
  };
  typedef std::vector<EventDuration> EventDurationList;

  // Builds a pretty printed ASCII tree showing the chronological order
  // of events.
  //
  // The indentation reflects hiearchy, with the duration of each indented
  // block noted on the right. The timestamp (tick count in milliseconds)
  // is noted in the left column.
  //
  // This detailed view of what went into servicing a request can be used
  // in logs, and is copy-pastable by users, for attaching to bug reports.
  //
  // Example A:
  //
  //     t=0: +Event1       [dt = 8]
  //     t=1:   +Event2     [dt = 0]
  //     t=1:      EventX
  //     t=1:   -Event2
  //     t=4:   +Event3     [dt = 2]
  //     t=6:   -Event3
  //     t=6:   +Event2     [dt = 1]
  //     t=7:   -Event2
  //     t=8:    EventY
  //     t=8: -Event1
  //
  // Here we can see that:
  //   - Event1 started at t=0 and ended at t=8; the duration was 8 time units.
  //   - Event2 took place while Event1 was in progress, and was repeated
  //     at t=1 and t=6.
  //   - EventX took place while (the first) Event2 was in progress.
  //   - Event3 started and ended at the same time, taking 0 time.
  //   - EventY took place right before Event1 finished, at t=8
  //
  // In general the rules are:
  //   - Log entries added by BeginEvent() are prefixed with a '+' and
  //     start an indentation block.
  //   - Log entries added by EndEvent() are prefixed with a '-' and
  //     finish an indentation block.
  //   - Log entries added by AddEvent() have no prefix.
  //   - Time units are given as milliseconds.
  //
  static std::string PrettyPrintAsEventTree(const LoadLog* log);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(LoadLogUtil);
};

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_UTIL_H_
