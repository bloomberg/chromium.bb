// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_EVENT_LOGGER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_EVENT_LOGGER_H_

#include <stdarg.h>   // va_list
#include <string>
#include <deque>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time.h"

namespace drive {

// EventLogger is used to expose text messages to chrome:drive-internals,
// for diagnosing the behavior of the Drive client.
class EventLogger {
 public:
  // Represents a single event log.
  struct Event {
    Event(int id, const std::string& what);
    int id;  // Monotonically increasing ID starting from 0.
    base::Time when;  // When the event occurred.
    std::string what;  // What happened.
  };

  // Creates an event logger that keeps the latest |history_size| events.
  explicit EventLogger(size_t history_size);
  ~EventLogger();

  // Logs a message using printf format.
  // Note that PRINTF_FORMAT should be (2, 3) instead of (1, 2) as this is a
  // C++ member function.
  void Log(const char* format, ...) PRINTF_FORMAT(2, 3);

  // Gets the list of latest events (the oldest event comes first).
  const std::deque<Event>& history() const { return history_; }

 private:
  std::deque<Event> history_;
  const size_t history_size_;
  int next_event_id_;

  DISALLOW_COPY_AND_ASSIGN(EventLogger);
};

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_EVENT_LOGGER_H_
