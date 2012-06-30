// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/performance_monitor_util.h"

#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "chrome/browser/performance_monitor/events.h"

namespace performance_monitor {
namespace util {

std::vector<MetricInfo> AggregateMetric(
    const std::vector<MetricInfo>& metric_infos,
    const base::Time& start,
    const base::TimeDelta& resolution) {
  std::vector<MetricInfo> results;
  // Ignore all the points before the aggregation start.
  std::vector<MetricInfo>::const_iterator it = metric_infos.begin();
  for (; it != metric_infos.end() && it->time < start; ++it) { }

  while (it != metric_infos.end()) {
    // Finds the beginning of the next aggregation window.
    int64 window_offset = (it->time - start) / resolution;
    base::Time window_start = start + (window_offset * resolution);
    base::Time window_end = window_start + resolution;
    base::Time last_sample_time = window_start;
    double integrated = 0.0;
    double metric_value = 0.0;

    // Aggregate the step function defined by the MetricInfos in |metric_infos|.
    while (it != metric_infos.end() && it->time <= window_end) {
      metric_value = it->value;
      integrated += metric_value * (it->time - last_sample_time).InSecondsF();
      last_sample_time = it->time;
      ++it;
    }
    if (it != metric_infos.end())
      metric_value = it->value;

    // If the window splits an area of the step function, split the aggregation
    // at the end of the window.
    integrated += metric_value * (window_end - last_sample_time).InSecondsF();
    double average = integrated / resolution.InSecondsF();
    results.push_back(MetricInfo(window_end, average));
  }
  return results;
}

scoped_ptr<Event> CreateExtensionInstallEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description) {
  events::ExtensionInstall event;
  event.event_type = EVENT_EXTENSION_INSTALL;
  event.time = static_cast<double>(time.ToInternalValue());
  event.extension_id = id;
  event.extension_name = name;
  event.extension_url = url;
  event.extension_location = location;
  event.extension_version = version;
  event.extension_description = description;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_EXTENSION_INSTALL, time, value.Pass()));
}

scoped_ptr<Event> CreateExtensionUninstallEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description) {
  events::ExtensionUninstall event;
  event.event_type = EVENT_EXTENSION_UNINSTALL;
  event.time = static_cast<double>(time.ToInternalValue());
  event.extension_id = id;
  event.extension_name = name;
  event.extension_url = url;
  event.extension_location = location;
  event.extension_version = version;
  event.extension_description = description;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_EXTENSION_UNINSTALL, time, value.Pass()));
}

scoped_ptr<Event> CreateExtensionUnloadEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description,
    const extension_misc::UnloadedExtensionReason& reason) {
  events::ExtensionUnload event;
  event.event_type = EVENT_EXTENSION_UNLOAD;
  event.time = static_cast<double>(time.ToInternalValue());
  event.extension_id = id;
  event.extension_name = name;
  event.extension_url = url;
  event.extension_location = location;
  event.extension_version = version;
  event.extension_description = description;
  event.unload_reason = reason;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_EXTENSION_UNLOAD, time, value.Pass()));
}

scoped_ptr<Event> CreateExtensionEnableEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description) {
  events::ExtensionEnable event;
  event.event_type = EVENT_EXTENSION_ENABLE;
  event.time = static_cast<double>(time.ToInternalValue());
  event.extension_id = id;
  event.extension_name = name;
  event.extension_url = url;
  event.extension_location = location;
  event.extension_version = version;
  event.extension_description = description;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_EXTENSION_ENABLE, time, value.Pass()));
}

scoped_ptr<Event> CreateExtensionUpdateEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description) {
  events::ExtensionUpdate event;
  event.event_type = EVENT_EXTENSION_UPDATE;
  event.time = static_cast<double>(time.ToInternalValue());
  event.extension_id = id;
  event.extension_name = name;
  event.extension_url = url;
  event.extension_location = location;
  event.extension_version = version;
  event.extension_description = description;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_EXTENSION_UPDATE, time, value.Pass()));
}

scoped_ptr<Event> CreateRendererFreezeEvent(
    const base::Time& time,
    const std::string& url) {
  events::RendererFreeze event;
  event.event_type = EVENT_RENDERER_FREEZE;
  event.time = static_cast<double>(time.ToInternalValue());
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_RENDERER_FREEZE, time, value.Pass()));
}

scoped_ptr<Event> CreateCrashEvent(
    const base::Time& time,
    const EventType& type,
    const std::string& url) {
  events::RendererFreeze event;
  event.event_type = type;
  event.time = static_cast<double>(time.ToInternalValue());
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(type, time, value.Pass()));
}

scoped_ptr<Event> CreateUncleanShutdownEvent(const base::Time& time) {
  events::UncleanShutdown event;
  event.event_type = EVENT_UNCLEAN_SHUTDOWN;
  event.time = static_cast<double>(time.ToInternalValue());
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_UNCLEAN_SHUTDOWN, time, value.Pass()));
}

scoped_ptr<Event> CreateChromeUpdateEvent(
    const base::Time& time,
    const std::string& previous_version,
    const std::string& current_version) {
  events::ChromeUpdate event;
  event.event_type = EVENT_CHROME_UPDATE;
  event.time = static_cast<double>(time.ToInternalValue());
  event.previous_version = previous_version;
  event.current_version = current_version;
  scoped_ptr<base::DictionaryValue> value = event.ToValue();
  return scoped_ptr<Event>(new Event(
      EVENT_CHROME_UPDATE, time, value.Pass()));
}

}  // namespace util
}  // namespace performance_monitor
