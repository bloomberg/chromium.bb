// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_UTIL_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_UTIL_H_

#include "base/time.h"
#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric_info.h"
#include "chrome/common/extensions/extension_constants.h"

namespace performance_monitor {
namespace util {

// Metric data can be either dense or sporadic, so AggregateMetric() normalizes
// the metric data in time. |metric_infos| must be sorted in increasing time.
// Put concisely, AggregateMetric() does sample rate conversion from irregular
// metric data points to a sample period of |resolution| beginning at |start|.
// Each sampling window starts and ends at an integer multiple away from
// |start| and data points are omitted if there are no points to resample.
std::vector<MetricInfo> AggregateMetric(
    const std::vector<MetricInfo>& metric_infos,
    const base::Time& start,
    const base::TimeDelta& resolution);

// These are a collection of methods designed to create an event to store the
// pertinent information, given all the fields. Please use these methods to
// create any PerformanceMonitor events, as this will ensure strong-typing
// guards that performance_monitor::Event() will not.
scoped_ptr<Event> CreateExtensionInstallEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description);

scoped_ptr<Event> CreateExtensionUnloadEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description,
    const extension_misc::UnloadedExtensionReason& reason);

scoped_ptr<Event> CreateExtensionUninstallEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description);

scoped_ptr<Event> CreateExtensionEnableEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description);

scoped_ptr<Event> CreateExtensionUpdateEvent(
    const base::Time& time,
    const std::string& id,
    const std::string& name,
    const std::string& url,
    const int& location,
    const std::string& version,
    const std::string& description);

scoped_ptr<Event> CreateRendererFreezeEvent(
    const base::Time& time,
    const std::string& url);

scoped_ptr<Event> CreateCrashEvent(
    const base::Time& time,
    const EventType& type,
    const std::string& url);

scoped_ptr<Event> CreateUncleanShutdownEvent(const base::Time& time);

scoped_ptr<Event> CreateChromeUpdateEvent(
    const base::Time& time,
    const std::string& old_version,
    const std::string& new_version);

}  // namespace util
}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_UTIL_H__
