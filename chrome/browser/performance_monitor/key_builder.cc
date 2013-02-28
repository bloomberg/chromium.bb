// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/key_builder.h"

#include "base/format_macros.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"

namespace performance_monitor {

namespace {

const char kDelimiter = '!';

// These values are used as the portion of the generated key which represents
// the event/metric type when inserting values in the database. We use an ASCII
// character as a mapping, rather than the enum of the metric or event itself,
// so that we can edit the MetricType and EventType enums as desired, without
// worrying about the integrity of the database.
//
// Once a character mapping has been set for a metric or event, do not change
// its value! New character mappings should be greater than 34 (ASCII characters
// below 32 have meaning, 33 is '!' - the database delimiter, and 34 is reserved
// for the 'Undefined' character mapping). Do not repeat values within the
// metric/event sets (repeated values between sets are okay).
//
// Deprecated keys: A key which is deprecated should be clearly marked as such,
// and its use discontinued. Do not remove the key from the listing! (Otherwise,
// a new metric may take its key and think the old data belongs to it.)

enum MetricKeyChar {
METRIC_UNDEFINED_KEY_CHAR = 34,
METRIC_CPU_USAGE_KEY_CHAR = 35,
METRIC_PRIVATE_MEMORY_USAGE_KEY_CHAR = 36,
METRIC_SHARED_MEMORY_USAGE_KEY_CHAR = 37,
METRIC_STARTUP_TIME_KEY_CHAR = 38,
METRIC_TEST_STARTUP_TIME_KEY_CHAR = 39,
METRIC_SESSION_RESTORE_TIME_KEY_CHAR = 40,
METRIC_PAGE_LOAD_TIME_KEY_CHAR = 41,
METRIC_NETWORK_BYTES_READ_KEY_CHAR = 42,
METRIC_NUMBER_OF_METRICS_KEY_CHAR = 255,
};

enum EventKeyChar {
EVENT_UNDEFINED_KEY_CHAR = 34,
EVENT_EXTENSION_INSTALL_KEY_CHAR = 35,
EVENT_EXTENSION_UNINSTALL_KEY_CHAR = 36,
EVENT_EXTENSION_UPDATE_KEY_CHAR = 37,
EVENT_EXTENSION_ENABLE_KEY_CHAR = 38,
EVENT_EXTENSION_DISABLE_KEY_CHAR = 39,
EVENT_CHROME_UPDATE_KEY_CHAR = 40,
EVENT_RENDERER_HANG_KEY_CHAR = 41,
EVENT_RENDERER_CRASH_KEY_CHAR = 42,
EVENT_RENDERER_KILLED_KEY_CHAR = 43,
EVENT_UNCLEAN_EXIT_KEY_CHAR = 44,
EVENT_NUMBER_OF_EVENTS_KEY_CHAR = 255,
};

// The position of different elements in the key for the event db.
enum EventKeyPosition {
  EVENT_TIME,  // The time the event was generated.
  EVENT_TYPE  // The type of event.
};

// The position of different elements in the key for the recent db.
enum RecentKeyPosition {
  RECENT_TIME,  // The time the stat was gathered.
  RECENT_TYPE,  // The unique identifier for the type of metric gathered.
  RECENT_ACTIVITY  // The unique identifier for the activity.
};

// The position of different elements in the key for the max value db.
enum MaxValueKeyPosition {
  MAX_VALUE_TYPE,  // The unique identifier for the type of metric gathered.
  MAX_VALUE_ACTIVITY  // The unique identifier for the activity.
};

// The position of different elements in the key for a metric db.
enum MetricKeyPosition {
  METRIC_TYPE,  // The unique identifier for the metric.
  METRIC_TIME,  // The time the stat was gathered.
  METRIC_ACTIVITY  // The unique identifier for the activity.
};

}  // namespace

RecentKey::RecentKey(const std::string& recent_time,
                     MetricType recent_type,
                     const std::string& recent_activity)
    : time(recent_time), type(recent_type), activity(recent_activity) {
}

RecentKey::~RecentKey() {
}

MetricKey::MetricKey(const std::string& metric_time,
                     MetricType metric_type,
                     const std::string& metric_activity)
    : time(metric_time), type(metric_type), activity(metric_activity) {
}

MetricKey::~MetricKey() {
}

KeyBuilder::KeyBuilder() {
  PopulateKeyMaps();
}

KeyBuilder::~KeyBuilder() {
}

void KeyBuilder::PopulateKeyMaps() {
  // Hard-code the generation of the map between event types and event key
  // character mappings.
  event_type_to_event_key_char_[EVENT_UNDEFINED] = EVENT_UNDEFINED_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_EXTENSION_INSTALL] =
      EVENT_EXTENSION_INSTALL_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_EXTENSION_UNINSTALL] =
      EVENT_EXTENSION_UNINSTALL_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_EXTENSION_UPDATE] =
      EVENT_EXTENSION_UPDATE_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_EXTENSION_ENABLE] =
      EVENT_EXTENSION_ENABLE_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_EXTENSION_DISABLE] =
      EVENT_EXTENSION_DISABLE_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_RENDERER_HANG] =
      EVENT_RENDERER_HANG_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_RENDERER_CRASH] =
      EVENT_RENDERER_CRASH_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_RENDERER_KILLED] =
      EVENT_RENDERER_KILLED_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_UNCLEAN_EXIT] =
      EVENT_UNCLEAN_EXIT_KEY_CHAR;
  event_type_to_event_key_char_[EVENT_NUMBER_OF_EVENTS] =
      EVENT_NUMBER_OF_EVENTS_KEY_CHAR;
  DCHECK(event_type_to_event_key_char_.size() == EVENT_NUMBER_OF_EVENTS);

  // Generate the reverse map for easy look-up between event character mappings
  // and event types.
  for (int i = static_cast<int>(EVENT_UNDEFINED);
       i <= static_cast<int>(EVENT_NUMBER_OF_EVENTS); ++i) {
    event_key_char_to_event_type_[event_type_to_event_key_char_[
        static_cast<EventType>(i)]] = static_cast<EventType>(i);
  }

  // Repeat the process for metrics.
  metric_type_to_metric_key_char_[METRIC_UNDEFINED] = METRIC_UNDEFINED_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_CPU_USAGE] = METRIC_CPU_USAGE_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_PRIVATE_MEMORY_USAGE] =
      METRIC_PRIVATE_MEMORY_USAGE_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_SHARED_MEMORY_USAGE] =
      METRIC_SHARED_MEMORY_USAGE_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_STARTUP_TIME] =
      METRIC_STARTUP_TIME_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_TEST_STARTUP_TIME] =
      METRIC_TEST_STARTUP_TIME_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_PAGE_LOAD_TIME] =
      METRIC_PAGE_LOAD_TIME_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_NETWORK_BYTES_READ] =
      METRIC_NETWORK_BYTES_READ_KEY_CHAR;
  metric_type_to_metric_key_char_[METRIC_NUMBER_OF_METRICS] =
      METRIC_NUMBER_OF_METRICS_KEY_CHAR;
  DCHECK(metric_type_to_metric_key_char_.size() == METRIC_NUMBER_OF_METRICS);

  for (int i = static_cast<int>(METRIC_UNDEFINED);
       i <= static_cast<int>(METRIC_NUMBER_OF_METRICS); ++i) {
    metric_key_char_to_metric_type_[metric_type_to_metric_key_char_[
        static_cast<MetricType>(i)]] = static_cast<MetricType>(i);
  }
}

std::string KeyBuilder::CreateActiveIntervalKey(const base::Time& time) {
    return StringPrintf("%016" PRId64, time.ToInternalValue());
}

std::string KeyBuilder::CreateMetricKey(const base::Time& time,
                                          const MetricType type,
                                          const std::string& activity) {
  return StringPrintf("%c%c%016" PRId64 "%c%s",
                      metric_type_to_metric_key_char_[type],
                      kDelimiter, time.ToInternalValue(),
                      kDelimiter, activity.c_str());
}

std::string KeyBuilder::CreateEventKey(const base::Time& time,
                                         const EventType type) {
  return StringPrintf("%016" PRId64 "%c%c",
                      time.ToInternalValue(), kDelimiter,
                      event_type_to_event_key_char_[type]);
}

std::string KeyBuilder::CreateRecentKey(const base::Time& time,
                                          const MetricType type,
                                          const std::string& activity) {
  return StringPrintf("%016" PRId64 "%c%c%c%s",
                      time.ToInternalValue(),
                      kDelimiter, metric_type_to_metric_key_char_[type],
                      kDelimiter, activity.c_str());
}

std::string KeyBuilder::CreateRecentMapKey(const MetricType type,
                                             const std::string& activity) {
  return StringPrintf("%s%c%c",
                      activity.c_str(),
                      kDelimiter, metric_type_to_metric_key_char_[type]);
}

std::string KeyBuilder::CreateMaxValueKey(const MetricType type,
                                            const std::string& activity) {
  return StringPrintf("%c%c%s",
                      metric_type_to_metric_key_char_[type],
                      kDelimiter, activity.c_str());
}

EventType KeyBuilder::EventKeyToEventType(const std::string& event_key) {
  std::vector<std::string> split;
  base::SplitString(event_key, kDelimiter, &split);
  DCHECK(split[EVENT_TYPE].size() == 1);
  return event_key_char_to_event_type_[
      static_cast<int>(split[EVENT_TYPE].at(0))];
}

RecentKey KeyBuilder::SplitRecentKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  DCHECK(split[RECENT_TYPE].size() == 1);
  return RecentKey(split[RECENT_TIME],
                   metric_key_char_to_metric_type_[
                       static_cast<int>(split[RECENT_TYPE].at(0))],
                   split[RECENT_ACTIVITY]);
}

MetricKey KeyBuilder::SplitMetricKey(const std::string& key) {
  std::vector<std::string> split;
  base::SplitString(key, kDelimiter, &split);
  DCHECK(split[METRIC_TYPE].size() == 1);
  return MetricKey(split[METRIC_TIME],
                   metric_key_char_to_metric_type_[
                       static_cast<int>(split[METRIC_TYPE].at(0))],
                   split[METRIC_ACTIVITY]);
}

}  // namespace performance_monitor
