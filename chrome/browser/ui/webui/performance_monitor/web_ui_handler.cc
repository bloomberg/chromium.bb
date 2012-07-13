// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/web_ui_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric_details.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/common/extensions/value_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

namespace performance_monitor {
namespace {

// Queries the performance monitor database for active intervals between
// |start| and |end| times and appends the results to |results|.
void DoGetActiveIntervals(ListValue* results,
                          const base::Time& start, const base::Time& end) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  std::vector<TimeRange> intervals = db->GetActiveIntervals(start, end);

  for (std::vector<TimeRange>::iterator it = intervals.begin();
       it != intervals.end(); ++it) {
    DictionaryValue* interval_value = new DictionaryValue();
    interval_value->SetDouble("start", it->start.ToJsTime());
    interval_value->SetDouble("end", it->end.ToJsTime());
    results->Append(interval_value);
  }
}

// Queries the performance monitor database for events of type |event_type|
// between |start| and |end| times and appends the results to |results|.
// TODO(mtytel): Add an internationalized longDescription field to each event.
void DoGetEvents(ListValue* results, EventType event_type,
                 const base::Time& start, const base::Time& end) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  std::vector<linked_ptr<Event> > events =
      db->GetEvents(event_type, start, end);

  for (std::vector<linked_ptr<Event> >::iterator it = events.begin();
       it != events.end(); ++it) {
    results->Append((*it)->data()->DeepCopy());
  }
}

// Queries the performance monitor database for metrics of type |metric_type|
// between |start| and |end| times and appends the results to |results|.
void DoGetMetric(ListValue* results,
                 MetricType metric_type,
                 const base::Time& start, const base::Time& end,
                 const base::TimeDelta& resolution) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  Database::MetricVectorMap metric_vector_map = db->GetStatsForMetricByActivity(
      MetricTypeToString(metric_type), start, end);

  linked_ptr<Database::MetricInfoVector> metric_vector =
      metric_vector_map[kProcessChromeAggregate];
  if (!metric_vector.get())
    metric_vector.reset(new Database::MetricInfoVector());

  Database::MetricInfoVector aggregated_metrics =
      util::AggregateMetric(*metric_vector, start, resolution);
  for (Database::MetricInfoVector::iterator it = aggregated_metrics.begin();
       it != aggregated_metrics.end(); ++it) {
    DictionaryValue* metric_value = new DictionaryValue();
    metric_value->SetDouble("time", it->time.ToJsTime());
    metric_value->SetDouble("value", it->value);
    results->Append(metric_value);
  }
}

}  // namespace

WebUIHandler::WebUIHandler() {}
WebUIHandler::~WebUIHandler() {}

void WebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getActiveIntervals",
      base::Bind(&WebUIHandler::HandleGetActiveIntervals,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllEventTypes",
      base::Bind(&WebUIHandler::HandleGetAllEventTypes,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getEvents",
      base::Bind(&WebUIHandler::HandleGetEvents,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getAllMetricTypes",
      base::Bind(&WebUIHandler::HandleGetAllMetricTypes,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getMetric",
      base::Bind(&WebUIHandler::HandleGetMetric,
                 base::Unretained(this)));
}

void WebUIHandler::ReturnResults(const std::string& function,
                                 const Value* results) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  web_ui()->CallJavascriptFunction(function, *results);
}

void WebUIHandler::HandleGetActiveIntervals(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(2u, args->GetSize());
  double double_time = 0.0;
  CHECK(args->GetDouble(0, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(1, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  ListValue* results = new ListValue();
  util::PostTaskToDatabaseThreadAndReply(
      base::Bind(&DoGetActiveIntervals, results, start, end),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "performance_monitor.getActiveIntervalsCallback",
                 base::Owned(results)));
}

void WebUIHandler::HandleGetAllEventTypes(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(0u, args->GetSize());
  ListValue results;
  for (int i = 0; i < EVENT_NUMBER_OF_EVENTS; ++i) {
    EventType event_type = static_cast<EventType>(i);
    DictionaryValue* event_type_info = new DictionaryValue();
    event_type_info->SetInteger("eventType", event_type);
    event_type_info->SetString("shortDescription",
                               EventTypeToString(event_type));
    results.Append(event_type_info);
  }

  ReturnResults("performance_monitor.getAllEventTypesCallback", &results);
}

void WebUIHandler::HandleGetEvents(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(3u, args->GetSize());
  double event = 0;
  CHECK(args->GetDouble(0, &event));
  EventType event_type = static_cast<EventType>(static_cast<int>(event));

  double double_time = 0.0;
  CHECK(args->GetDouble(1, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(2, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  DictionaryValue* results = new DictionaryValue();
  ListValue* points_results = new ListValue();
  results->Set("points", points_results);
  results->SetInteger("type", event_type);
  util::PostTaskToDatabaseThreadAndReply(
      base::Bind(&DoGetEvents, points_results, event_type, start, end),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "performance_monitor.getEventsCallback",
                 base::Owned(results)));
}

void WebUIHandler::HandleGetAllMetricTypes(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(0u, args->GetSize());
  ListValue results;
  for (int i = 0; i < METRIC_NUMBER_OF_METRICS; ++i) {
    MetricType metric_type = static_cast<MetricType>(i);
    DictionaryValue* metric_type_info = new DictionaryValue();
    metric_type_info->SetInteger("metricType", metric_type);
    metric_type_info->SetString("shortDescription",
                                MetricTypeToString(metric_type));
    results.Append(metric_type_info);
  }

  ReturnResults("performance_monitor.getAllMetricTypesCallback", &results);
}

void WebUIHandler::HandleGetMetric(const ListValue* args) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  CHECK_EQ(4u, args->GetSize());
  double metric = 0;
  CHECK(args->GetDouble(0, &metric));
  MetricType metric_type = static_cast<MetricType>(static_cast<int>(metric));

  double double_time = 0.0;
  CHECK(args->GetDouble(1, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(2, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  double resolution_in_milliseconds = 0;
  CHECK(args->GetDouble(3, &resolution_in_milliseconds));
  base::TimeDelta resolution =
      base::TimeDelta::FromMilliseconds(resolution_in_milliseconds);

  DictionaryValue* results = new DictionaryValue();
  results->SetInteger("type", metric_type);
  ListValue* points_results = new ListValue();
  results->Set("points", points_results);
  util::PostTaskToDatabaseThreadAndReply(
      base::Bind(&DoGetMetric, points_results, metric_type,
                 start, end, resolution),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "performance_monitor.getMetricCallback",
                 base::Owned(results)));
}

}  // namespace performance_monitor
