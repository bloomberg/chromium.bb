// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_HANDLER_H_

#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
class Time;
class Value;
}  // namespace base

namespace performance_monitor {

// This class handles messages to and from the performance monitor page.
// Incoming calls are handled by the Handle* functions and callbacks are made
// from ReturnResults functions.
class PerformanceMonitorHandler
    : public content::WebUIMessageHandler,
      public base::SupportsWeakPtr<PerformanceMonitorHandler> {
 public:
  PerformanceMonitorHandler();

 private:
  virtual ~PerformanceMonitorHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // Returns |results| through the given |callback| string.
  void ReturnResults(const std::string& callback, const base::Value* results);

  // Callback for the "getActiveIntervals" message.
  // |args| contains a start and an end time.
  void HandleGetActiveIntervals(const base::ListValue* args);

  // Callback for the "getFlagEnabled" message.
  // |args| is unused.
  void HandleGetFlagEnabled(const base::ListValue* args);

  // Callback for the "getAggregationTypes" message.
  // |args| is unused.
  void HandleGetAggregationTypes(const base::ListValue* args);

  // Callback for the "getEventTypes" message.
  // |args| is unused.
  void HandleGetEventTypes(const base::ListValue* args);

  // Callback for the "getEvents" message.
  // |args| contains an EventType id to collect and a start and end time.
  void HandleGetEvents(const base::ListValue* args);

  // Callback for the "getMetricTypes" message.
  // |args| is unused.
  void HandleGetMetricTypes(const base::ListValue* args);

  // Callback for the "getMetrics" message.
  // |args| contains a list of MetricTypes to collect, a start and end time,
  // a time resolution (defining the spacing of metric samples returned),
  // and an aggregation method.
  void HandleGetMetrics(const base::ListValue* args);

  DISALLOW_COPY_AND_ASSIGN(PerformanceMonitorHandler);
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_UI_WEBUI_PERFORMANCE_MONITOR_PERFORMANCE_MONITOR_HANDLER_H_
