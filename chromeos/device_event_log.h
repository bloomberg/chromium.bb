// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DEVICE_EVENT_LOG_H_
#define CHROMEOS_DEVICE_EVENT_LOG_H_

#include <cstring>
#include <sstream>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {

// These macros can be used to log chromeos device related events.
// The following values should be used for |level| in these macros:
//  ERROR Unexpected events, or device level failures. Use sparingly.
//  USER  Events initiated directly by a user (or Chrome) action.
//  EVENT Default event type.
//  DEBUG Debugging details that are usually not interesting.
// Examples:
//  NET_LOG(EVENT) << "NetworkState Changed " << name << ": " << state;
//  POWER_LOG(USER) << "Suspend requested";

#define NET_LOG(level)                                       \
  DEVICE_LOG(::chromeos::device_event_log::LOG_TYPE_NETWORK, \
             ::chromeos::device_event_log::LOG_LEVEL_##level)
#define POWER_LOG(level)                                   \
  DEVICE_LOG(::chromeos::device_event_log::LOG_TYPE_POWER, \
             ::chromeos::device_event_log::LOG_LEVEL_##level)

// Generally prefer the above macros unless |level| is not constant.

#define DEVICE_LOG(type, level)                                   \
  ::chromeos::device_event_log::internal::DeviceEventLogInstance( \
      __FILE__, __LINE__, type, level).stream()

namespace device_event_log {

// Used to specify the type of event.
enum LogType {
  // Shill / network configuration related events.
  LOG_TYPE_NETWORK,
  // Power manager related events.
  LOG_TYPE_POWER,
  // Use this in GetAsString for non NETWORK events (which are more frequent).
  LOG_TYPE_NON_NETWORK,
  // Use this in GetAsString for all events.
  LOG_TYPE_ALL,
};

// Used to specify the detail level for logging. In GetAsString, used to
// specify the maximum detail level (i.e. EVENT will include USER and ERROR).
// See top-level comment for guidelines for each type.
enum LogLevel {
  LOG_LEVEL_ERROR = 0,
  LOG_LEVEL_USER = 1,
  LOG_LEVEL_EVENT = 2,
  LOG_LEVEL_DEBUG = 3
};

// Used to specify which order to output event entries in GetAsString.
enum StringOrder { OLDEST_FIRST, NEWEST_FIRST };

// Initializes / shuts down device event logging. If |max_entries| = 0 the
// default value will be used.
CHROMEOS_EXPORT void Initialize(size_t max_entries);
CHROMEOS_EXPORT void Shutdown();

// If the global instance is initialized, adds an entry to it. Regardless of
// whether the global instance was intitialzed, this logs the event to
// LOG(ERROR) if |type| = ERROR or VLOG(1) otherwise.
CHROMEOS_EXPORT void AddEntry(const char* file,
                              int line,
                              LogType type,
                              LogLevel level,
                              const std::string& event);

// For backwards compatibility with network_event_log. Combines |event| and
// |description| and calls AddEntry().
CHROMEOS_EXPORT void AddEntryWithDescription(const char* file,
                                             int line,
                                             LogType type,
                                             LogLevel level,
                                             const std::string& event,
                                             const std::string& description);

// Outputs the log to a formatted string.
// |order| determines which order to output the events.
// |format| is a string that determines which elements to show. Elements
// must be comma-separated, e.g. "time,desc".
// Note: order of the format strings does not affect the output.
//  "time" - Include a timestamp.
//  "file" - Include file and line number.
//  "type" - Include the event type.
//  "html" - Include html tags.
//  "json" - Return as JSON format
// Only events matching |log_type| are included in the output.
// Only events with |log_level| <= |max_level| are included in the output.
// If |max_events| > 0, limits how many events are output.
// If |json| is specified, returns a JSON list of dictionaries containing time,
// level, file, event, and description.
CHROMEOS_EXPORT std::string GetAsString(StringOrder order,
                                        const std::string& format,
                                        LogType log_type,
                                        LogLevel max_level,
                                        size_t max_events);

CHROMEOS_EXPORT extern const LogLevel kDefaultLogLevel;

namespace internal {

class CHROMEOS_EXPORT DeviceEventLogInstance {
 public:
  DeviceEventLogInstance(const char* file,
                         int line,
                         device_event_log::LogType type,
                         device_event_log::LogLevel level);
  ~DeviceEventLogInstance();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  const int line_;
  device_event_log::LogType type_;
  device_event_log::LogLevel level_;
  std::ostringstream stream_;

  DISALLOW_COPY_AND_ASSIGN(DeviceEventLogInstance);
};

}  // namespace internal

}  // namespace device_event_log

}  // namespace chromeos

#endif  // CHROMEOS_DEVICE_EVENT_LOG_H_
