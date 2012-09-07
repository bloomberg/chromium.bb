// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_KEY_BUILDER_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_KEY_BUILDER_H_

#include <map>

#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric.h"

namespace performance_monitor {

struct RecentKey {
  RecentKey(const std::string& recent_time,
            MetricType recent_type,
            const std::string& recent_activity);
  ~RecentKey();

  const std::string time;
  const MetricType type;
  const std::string activity;
};

struct MaxValueKey {
  MaxValueKey(MetricType max_value_type,
              const std::string& max_value_activity)
      : type(max_value_type),
        activity(max_value_activity) {}
  ~MaxValueKey() {}

  const MetricType type;
  const std::string activity;
};

struct MetricKey {
  MetricKey(const std::string& metric_time,
            MetricType metric_type,
            const std::string& metric_activity);
  ~MetricKey();

  const std::string time;
  const MetricType type;
  const std::string activity;
};

// This class is responsible for building the keys which are used internally by
// PerformanceMonitor's database. These keys should only be referenced by the
// database, and should not be used externally.
class KeyBuilder {
 public:
  KeyBuilder();
  ~KeyBuilder();

  // Key Creation: Create the keys for different databases. The schemas are
  // listed with the methods. A '-' in the schema represents kDelimeter.

  // Key Schema: <Time>
  std::string CreateActiveIntervalKey(const base::Time& time);

  // Key Schema: <Metric>-<Time>-<Activity>
  std::string CreateMetricKey(const base::Time& time,
                              const MetricType type,
                              const std::string& activity);

  // Key Schema: <Time>-<Event Type>
  std::string CreateEventKey(const base::Time& time, const EventType type);

  // Key Schema: <Time>-<Metric>-<Activity>
  std::string CreateRecentKey(const base::Time& time,
                              const MetricType type,
                              const std::string& activity);

  // Key Schema: <Activity>-<Metric>
  std::string CreateRecentMapKey(const MetricType type,
                                 const std::string& activity);

  std::string CreateMaxValueKey(const MetricType type,
                                const std::string& activity);

  EventType EventKeyToEventType(const std::string& key);
  RecentKey SplitRecentKey(const std::string& key);
  MetricKey SplitMetricKey(const std::string& key);

 private:
  // Populate the maps from [Event, Metric]Type to key characters.
  void PopulateKeyMaps();

  std::map<EventType, int> event_type_to_event_key_char_;
  std::map<int, EventType> event_key_char_to_event_type_;
  std::map<MetricType, int> metric_type_to_metric_key_char_;
  std::map<int, MetricType> metric_key_char_to_metric_type_;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_KEY_BUILDER_H_
