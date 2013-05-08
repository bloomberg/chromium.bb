// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_APIS_EVENT_LOGGER_H_
#define CHROME_BROWSER_GOOGLE_APIS_EVENT_LOGGER_H_

#include <stdarg.h>   // va_list
#include <deque>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/time.h"

namespace google_apis {

// The default history size used by EventLogger.
const int kDefaultHistorySize = 1000;

// EventLogger is used to collect and expose text messages for diagnosing
// behaviors of Google APIs stuff. For instance, the collected messages are
// exposed to chrome:drive-internals.
class EventLogger {
 public:
  // Represents a single event log.
  struct Event {
    Event(int id, const std::string& what);
    int id;  // Monotonically increasing ID starting from 0.
    base::Time when;  // When the event occurred.
    std::string what;  // What happened.
  };

  // Creates an event logger that keeps the latest kDefaultHistorySize events.
  EventLogger();
  ~EventLogger();

  // Logs a message using printf format.
  // Can be called from any thread as long as the object is alive.
  // Note that PRINTF_FORMAT should be (2, 3) instead of (1, 2) as this is a
  // C++ member function.
  void Log(const char* format, ...) PRINTF_FORMAT(2, 3);

  // Sets the history size. The existing history is cleared.
  // Can be called from any thread as long as the object is alive.
  void SetHistorySize(size_t history_size);

  // Gets the list of latest events (the oldest event comes first).
  // Can be called from any thread as long as the object is alive.
  std::vector<Event> GetHistory();

 private:
  std::deque<Event> history_;  // guarded by lock_.
  size_t history_size_;  // guarded by lock_.
  int next_event_id_;  // guarded by lock_.
  base::Lock lock_;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

}  // namespace google_apis

#endif  // CHROME_BROWSER_GOOGLE_APIS_EVENT_LOGGER_H_
