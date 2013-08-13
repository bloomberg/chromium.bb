// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
#define CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_

#include <deque>
#include <string>

#include "base/basictypes.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "chromeos/chromeos_export.h"

namespace base {
class Value;
}

namespace chromeos {

// Namespace for functions for logging network events.
namespace network_event_log {

// Used to determine which order to output event entries in GetAsString.
enum StringOrder {
  OLDEST_FIRST,
  NEWEST_FIRST
};

// Used to set the detail level for logging.
enum LogLevel {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_USER = 1,
  LOG_LEVEL_EVENT = 2,
  LOG_LEVEL_DEBUG = 3
};

// Default log level.
CHROMEOS_EXPORT extern const LogLevel kDefaultLogLevel;

// Initializes / shuts down network event logging. Calling Initialize more than
// once will reset the log.
CHROMEOS_EXPORT void Initialize();
CHROMEOS_EXPORT void Shutdown();

// Returns true if network event logging has been initialized.
CHROMEOS_EXPORT bool IsInitialized();

namespace internal {

// Gets the maximum number of log entries.
CHROMEOS_EXPORT size_t GetMaxLogEntries();

// Sets the maximum number of entries to something other than the default. If
// |max_entries| is less than the current maximum number of entries, this will
// delete any existing entries in excess of |max_entries|.
CHROMEOS_EXPORT void SetMaxLogEntries(size_t max_entries);

// Adds an entry to the event log at level specified by |log_level|.
// A maximum number of events are recorded after which new events replace
// old ones. Error events are prioritized such that error events will only be
// deleted if more than least half of the entries are errors (at which point
// the oldest error entry will be replaced). Does nothing unless Initialize()
// has been called. NOTE: Generally use NET_LOG instead.
CHROMEOS_EXPORT void AddEntry(const char* file,
                              int file_line,
                              LogLevel log_level,
                              const std::string& event,
                              const std::string& description);

}  // namespace internal

// Outputs the log to a formatted string.
// |order| determines which order to output the events.
// |format| is a string that determines which elements to show. Elements
// must be comma-separated, e.g. "time,desc".
// Note: order of the format strings does not affect the output.
//  "time" - Include a timestamp.
//  "file" - Include file and line number.
//  "desc" - Include the description.
//  "html" - Include html tags.
//  "json" - Return as JSON format
// Only events with |log_level| <= |max_level| are included in the output.
// If |max_events| > 0, limits how many events are output.
// If |json| is specified, returns a JSON list of dictionaries containing time,
// level, file, event, and description.
CHROMEOS_EXPORT std::string GetAsString(StringOrder order,
                                        const std::string& format,
                                        LogLevel max_level,
                                        size_t max_events);

// Helper function for displaying a value as a string.
CHROMEOS_EXPORT std::string ValueAsString(const base::Value& value);

// Errors
#define NET_LOG_ERROR(event, desc) NET_LOG_LEVEL(                       \
    ::chromeos::network_event_log::LOG_LEVEL_ERROR, event, desc)

// Chrome initiated events, e.g. connection requests, scan requests,
// configuration removal (either from the UI or from ONC policy application).
#define NET_LOG_USER(event, desc) NET_LOG_LEVEL(                        \
    ::chromeos::network_event_log::LOG_LEVEL_USER, event, desc)

// Important events, e.g. state updates
#define NET_LOG_EVENT(event, desc) NET_LOG_LEVEL(                       \
    ::chromeos::network_event_log::LOG_LEVEL_EVENT, event, desc)

// Non-essential debugging events
#define NET_LOG_DEBUG(event, desc) NET_LOG_LEVEL(                       \
    ::chromeos::network_event_log::LOG_LEVEL_DEBUG, event, desc)

// Macro to include file and line number info in the event log.
#define NET_LOG_LEVEL(log_level, event, description)            \
  ::chromeos::network_event_log::internal::AddEntry(            \
      __FILE__, __LINE__, log_level, event, description)

}  // namespace network_event_log

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_NETWORK_EVENT_LOG_H_
