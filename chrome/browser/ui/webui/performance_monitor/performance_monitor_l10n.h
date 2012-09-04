// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_

#include <string>

#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"

namespace performance_monitor {

// Event-Related
string16 GetLocalizedStringFromEventCategory(const EventCategory category);
string16 GetLocalizedStringForEventCategoryDescription(
    const EventCategory category);
string16 GetLocalizedStringFromEventType(const EventType type);
string16 GetLocalizedStringForEventTypeDescription(const EventType type);
string16 GetLocalizedStringForEventTypeMouseover(const EventType type);
string16 GetLocalizedStringFromEventProperty(const std::string& key);

// Metric-Related
string16 GetLocalizedStringFromMetricCategory(const MetricCategory category);
string16 GetLocalizedStringForMetricCategoryDescription(
    const MetricCategory category);
string16 GetLocalizedStringFromMetricType(const MetricType type);
string16 GetLocalizedStringForMetricTypeDescription(const MetricType type);

// Miscellaneous
string16 GetLocalizedStringFromUnit(const Unit unit);

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_L10N_H_
