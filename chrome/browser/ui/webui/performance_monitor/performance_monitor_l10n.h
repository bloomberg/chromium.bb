// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_

#include <string>

#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_util.h"

namespace performance_monitor {

// Aggregation-Related
base::string16 GetLocalizedStringFromAggregationMethod(
    const AggregationMethod method);
base::string16 GetLocalizedStringForAggregationMethodDescription(
    const AggregationMethod method);

// Event-Related
base::string16 GetLocalizedStringFromEventCategory(
    const EventCategory category);
base::string16 GetLocalizedStringForEventCategoryDescription(
    const EventCategory category);
base::string16 GetLocalizedStringFromEventType(const EventType type);
base::string16 GetLocalizedStringForEventTypeDescription(const EventType type);
base::string16 GetLocalizedStringForEventTypeMouseover(const EventType type);
base::string16 GetLocalizedStringFromEventProperty(const std::string& key);

// Metric-Related
base::string16 GetLocalizedStringFromMetricCategory(
    const MetricCategory category);
base::string16 GetLocalizedStringForMetricCategoryDescription(
    const MetricCategory category);
base::string16 GetLocalizedStringFromMetricType(const MetricType type);
base::string16 GetLocalizedStringForMetricTypeDescription(
    const MetricType type);

// Miscellaneous
base::string16 GetLocalizedStringFromUnit(const Unit unit);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_
