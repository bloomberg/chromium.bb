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
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"
#include "chrome/common/extensions/value_builder.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

using content::BrowserThread;

namespace performance_monitor {
namespace {

std::set<MetricType> GetMetricSetForCategory(MetricCategory category) {
  std::set<MetricType> metric_set;
  switch (category) {
    case METRIC_CATEGORY_CPU:
      metric_set.insert(METRIC_CPU_USAGE);
      break;
    case METRIC_CATEGORY_MEMORY:
      metric_set.insert(METRIC_SHARED_MEMORY_USAGE);
      metric_set.insert(METRIC_PRIVATE_MEMORY_USAGE);
      break;
    case METRIC_CATEGORY_TIMING:
      metric_set.insert(METRIC_STARTUP_TIME);
      metric_set.insert(METRIC_TEST_STARTUP_TIME);
      metric_set.insert(METRIC_SESSION_RESTORE_TIME);
      metric_set.insert(METRIC_PAGE_LOAD_TIME);
      break;
    case METRIC_CATEGORY_NETWORK:
      metric_set.insert(METRIC_NETWORK_BYTES_READ);
      break;
    default:
      NOTREACHED();
  }
  return metric_set;
}

std::set<EventType> GetEventSetForCategory(EventCategory category) {
  std::set<EventType> event_set;
  switch (category) {
    case EVENT_CATEGORY_EXTENSIONS:
      event_set.insert(EVENT_EXTENSION_INSTALL);
      event_set.insert(EVENT_EXTENSION_UNINSTALL);
      event_set.insert(EVENT_EXTENSION_UPDATE);
      event_set.insert(EVENT_EXTENSION_ENABLE);
      event_set.insert(EVENT_EXTENSION_DISABLE);
      break;
    case EVENT_CATEGORY_CHROME:
      event_set.insert(EVENT_CHROME_UPDATE);
      break;
    case EVENT_CATEGORY_EXCEPTIONS:
      event_set.insert(EVENT_RENDERER_FREEZE);
      event_set.insert(EVENT_RENDERER_CRASH);
      event_set.insert(EVENT_KILLED_BY_OS_CRASH);
      event_set.insert(EVENT_UNCLEAN_EXIT);
      break;
    default:
      NOTREACHED();
  }
  return event_set;
}

Unit GetUnitForMetricCategory(MetricCategory category) {
  switch (category) {
    case METRIC_CATEGORY_CPU:
      return UNIT_PERCENT;
    case METRIC_CATEGORY_MEMORY:
      return UNIT_BYTES;
    case METRIC_CATEGORY_TIMING:
      return UNIT_MICROSECONDS;
    case METRIC_CATEGORY_NETWORK:
      return UNIT_BYTES;
    default:
      NOTREACHED();
  }
  return UNIT_UNDEFINED;
}

// Returns a list of metric details, with one entry per metric. Metric details
// are dictionaries which contain the id representing the metric and localized
// strings for the metric name and metric description.
scoped_ptr<ListValue> GetMetricDetailsForCategory(MetricCategory category) {
  scoped_ptr<ListValue> value(new ListValue());
  std::set<MetricType> metric_set = GetMetricSetForCategory(category);
  for (std::set<MetricType>::const_iterator iter = metric_set.begin();
       iter != metric_set.end(); ++iter) {
    DictionaryValue* metric_details = new DictionaryValue();
    metric_details->SetInteger("metricId", *iter);
    metric_details->SetString(
        "name", GetLocalizedStringFromMetricType(*iter));
    metric_details->SetString(
        "description", GetLocalizedStringForMetricTypeDescription(*iter));
    value->Append(metric_details);
  }
  return value.Pass();
}

// Returns a dictionary for the metric category. Metric categories contain
// an id representing the category; localized strings for the category name,
// the default unit in which the category is measured, and the category's
// description; and the metric details for each metric type in the category.
scoped_ptr<DictionaryValue> GetMetricCategory(MetricCategory category) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetInteger("metricCategoryId", category);
  value->SetString(
      "name", GetLocalizedStringFromMetricCategory(category));
  value->SetString(
      "unit",
      GetLocalizedStringFromUnit(GetUnitForMetricCategory(category)));
  value->SetString(
      "description",
      GetLocalizedStringForMetricCategoryDescription(category));
  value->Set("details", GetMetricDetailsForCategory(category).release());
  return value.Pass();
}

// Returns a list of event types, with one entry per event. Event types
// are dictionaries which contain the id representing the event and localized
// strings for the event name, event description, and a title suitable for a
// mouseover popup.
scoped_ptr<ListValue> GetEventTypesForCategory(EventCategory category) {
  scoped_ptr<ListValue> value(new ListValue());
  std::set<EventType> event_set = GetEventSetForCategory(category);
  for (std::set<EventType>::const_iterator iter = event_set.begin();
       iter != event_set.end(); ++iter) {
    DictionaryValue* event_details = new DictionaryValue();
    event_details->SetInteger("eventId", *iter);
    event_details->SetString(
        "name", GetLocalizedStringFromEventType(*iter));
    event_details->SetString(
        "description", GetLocalizedStringForEventTypeDescription(*iter));
    event_details->SetString(
        "popupTitle", GetLocalizedStringForEventTypeMouseover(*iter));
    value->Append(event_details);
  }
  return value.Pass();
}

// Returns a dictionary for the event category. Event categories contain an
// id representing the category, localized strings for the event name and
// event description, and event details for each event type in the category.
scoped_ptr<DictionaryValue> GetEventCategory(EventCategory category) {
  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetInteger("eventCategoryId", category);
  value->SetString(
      "name", GetLocalizedStringFromEventCategory(category));
  value->SetString(
      "description",
      GetLocalizedStringForEventCategoryDescription(category));
  value->Set("details", GetEventTypesForCategory(category).release());
  return value.Pass();
}

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

// Queries the PerformanceMonitor database for events of type |event_type|
// between |start| and |end| times, creates a new event with localized keys
// for display, and appends the results to |results|.
void DoGetEvents(ListValue* results, EventType event_type,
                 const base::Time& start, const base::Time& end) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  Database::EventVector events = db->GetEvents(event_type, start, end);

  for (Database::EventVector::iterator event = events.begin();
       event != events.end(); ++event) {
    DictionaryValue* localized_event = new DictionaryValue();

    for (DictionaryValue::key_iterator key = (*event)->data()->begin_keys();
         key != (*event)->data()->end_keys(); ++key) {
      std::string localized_key;

      Value* value = NULL;

      // The property 'eventType' is used computationally on the UI side (and
      // not displayed), so it does not need any localization.
      if (*key == "eventType") {
        Value* old_value = NULL;
        (*event)->data()->Get(*key, &old_value);
        value = old_value->DeepCopy();
      } else if (*key == "time") {
        // The property 'time' is also used computationally, but must be
        // converted to JS-style time.
        double time = 0.0;
        if (!(*event)->data()->GetDouble(std::string("time"), &time)) {
          LOG(ERROR) << "Failed to get 'time' field from event.";
          continue;
        }
        value = Value::CreateDoubleValue(
            base::Time::FromInternalValue(static_cast<int64>(time)).ToJsTime());
      } else {
        // All other values are user-facing, so we create a new value for
        // localized display.
        DictionaryValue* localized_value = new DictionaryValue();
        localized_value->SetString("displayString",
                                   GetLocalizedStringFromEventProperty(*key));
        Value* old_value = NULL;
        (*event)->data()->Get(*key, &old_value);
        localized_value->SetWithoutPathExpansion("value",
                                                 old_value->DeepCopy());
        value = static_cast<Value*>(localized_value);
      }

      localized_event->SetWithoutPathExpansion(*key, value);
    }
    results->Append(localized_event);
  }
}

// Queries the performance monitor database for metrics of type |metric_type|
// between |start| and |end| times and appends the results to |results|.
void DoGetMetric(DictionaryValue* results,
                 MetricType metric_type,
                 const base::Time& start, const base::Time& end,
                 const base::TimeDelta& resolution) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  // results->SetDouble("maxValue", db->GetMaxValueForMetric(metric_type));
  results->SetDouble("maxValue", 10.0);
  results->SetInteger("metricId", static_cast<int>(metric_type));

  ListValue* metric_list = new ListValue();
  results->Set("metrics", metric_list);
  Database::MetricVectorMap metric_vector_map =
      db->GetStatsForMetricByActivity(metric_type, start, end);

  linked_ptr<Database::MetricVector> metric_vector =
      metric_vector_map[kProcessChromeAggregate];
  if (!metric_vector.get())
    metric_vector.reset(new Database::MetricVector());

  Database::MetricVector aggregated_metrics =
      util::AggregateMetric(*metric_vector, start, resolution);
  for (Database::MetricVector::const_iterator it = aggregated_metrics.begin();
       it != aggregated_metrics.end(); ++it) {
    DictionaryValue* metric_value = new DictionaryValue();
    metric_value->SetDouble("time", it->time.ToJsTime());
    metric_value->SetDouble("value", it->value);
    metric_list->Append(metric_value);
  }
}

}  // namespace

WebUIHandler::WebUIHandler() {
  // If we are not running the --run-performance-monitor flag, we will not have
  // started PerformanceMonitor.
  if (!PerformanceMonitor::initialized())
    PerformanceMonitor::GetInstance()->Start();
}

WebUIHandler::~WebUIHandler() {}

void WebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getActiveIntervals",
      base::Bind(&WebUIHandler::HandleGetActiveIntervals,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEventTypes",
      base::Bind(&WebUIHandler::HandleGetEventTypes,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEvents",
      base::Bind(&WebUIHandler::HandleGetEvents,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getMetricTypes",
      base::Bind(&WebUIHandler::HandleGetMetricTypes,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getMetric",
      base::Bind(&WebUIHandler::HandleGetMetric,
                 AsWeakPtr()));
}

void WebUIHandler::ReturnResults(const std::string& function,
                                 const Value* results) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  web_ui()->CallJavascriptFunction(function, *results);
}

void WebUIHandler::HandleGetActiveIntervals(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK_EQ(2u, args->GetSize());
  double double_time = 0.0;
  CHECK(args->GetDouble(0, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(1, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  ListValue* results = new ListValue();
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetActiveIntervals, results, start, end),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getActiveIntervalsCallback",
                 base::Owned(results)));
}

void WebUIHandler::HandleGetEventTypes(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK_EQ(0u, args->GetSize());
  ListValue results;
  for (int i = 0; i < EVENT_CATEGORY_NUMBER_OF_CATEGORIES; ++i)
    results.Append(GetEventCategory(static_cast<EventCategory>(i)).release());

  ReturnResults("PerformanceMonitor.getEventTypesCallback", &results);
}

void WebUIHandler::HandleGetEvents(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  results->SetInteger("eventId", static_cast<int>(event_type));
  ListValue* events = new ListValue();
  results->Set("events", events);

  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetEvents, events, event_type, start, end),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getEventsCallback",
                 base::Owned(results)));
}

void WebUIHandler::HandleGetMetricTypes(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CHECK_EQ(0u, args->GetSize());
  ListValue results;
  for (int i = 0; i < METRIC_CATEGORY_NUMBER_OF_CATEGORIES; ++i)
    results.Append(GetMetricCategory(static_cast<MetricCategory>(i)).release());

  ReturnResults("PerformanceMonitor.getMetricTypesCallback", &results);
}

void WebUIHandler::HandleGetMetric(const ListValue* args) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetMetric, results, metric_type,
                 start, end, resolution),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getMetricCallback",
                 base::Owned(results)));
}

}  // namespace performance_monitor
