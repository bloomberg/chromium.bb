// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/event.h"
#include "chrome/browser/performance_monitor/metric.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_l10n.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_constants.h"
#include "chrome/browser/ui/webui/performance_monitor/performance_monitor_ui_util.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "extensions/common/value_builder.h"

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
      event_set.insert(EVENT_RENDERER_HANG);
      event_set.insert(EVENT_RENDERER_CRASH);
      event_set.insert(EVENT_RENDERER_KILLED);
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
      return UNIT_MEGABYTES;
    case METRIC_CATEGORY_TIMING:
      return UNIT_SECONDS;
    case METRIC_CATEGORY_NETWORK:
      return UNIT_MEGABYTES;
    default:
      NOTREACHED();
  }
  return UNIT_UNDEFINED;
}

MetricCategory GetCategoryForMetric(MetricType type) {
  switch (type) {
    case METRIC_CPU_USAGE:
      return METRIC_CATEGORY_CPU;
    case METRIC_SHARED_MEMORY_USAGE:
    case METRIC_PRIVATE_MEMORY_USAGE:
      return METRIC_CATEGORY_MEMORY;
    case METRIC_STARTUP_TIME:
    case METRIC_TEST_STARTUP_TIME:
    case METRIC_SESSION_RESTORE_TIME:
    case METRIC_PAGE_LOAD_TIME:
      return METRIC_CATEGORY_TIMING;
    case METRIC_NETWORK_BYTES_READ:
      return METRIC_CATEGORY_NETWORK;
    default:
      NOTREACHED();
  }
  return METRIC_CATEGORY_NUMBER_OF_CATEGORIES;
}

Unit GetUnitForMetricType(MetricType type) {
  switch (type) {
    case METRIC_CPU_USAGE:
      return UNIT_PERCENT;
    case METRIC_SHARED_MEMORY_USAGE:
    case METRIC_PRIVATE_MEMORY_USAGE:
    case METRIC_NETWORK_BYTES_READ:
      return UNIT_BYTES;
    case METRIC_STARTUP_TIME:
    case METRIC_TEST_STARTUP_TIME:
    case METRIC_SESSION_RESTORE_TIME:
    case METRIC_PAGE_LOAD_TIME:
      return UNIT_MICROSECONDS;
    default:
      NOTREACHED();
  }
  return UNIT_UNDEFINED;
}

// Returns a dictionary for the aggregation method. Aggregation strategies
// contain an id representing the method, and localized strings for the
// method name and method description.
scoped_ptr<base::DictionaryValue> GetAggregationMethod(
    AggregationMethod method) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetInteger("id", method);
  value->SetString("name", GetLocalizedStringFromAggregationMethod(method));
  value->SetString(
      "description",
      GetLocalizedStringForAggregationMethodDescription(method));
  return value.Pass();
}

// Returns a list of metric details, with one entry per metric. Metric details
// are dictionaries which contain the id representing the metric and localized
// strings for the metric name and metric description.
scoped_ptr<base::ListValue> GetMetricDetailsForCategory(
    MetricCategory category) {
  scoped_ptr<base::ListValue> value(new base::ListValue());
  std::set<MetricType> metric_set = GetMetricSetForCategory(category);
  for (std::set<MetricType>::const_iterator iter = metric_set.begin();
       iter != metric_set.end(); ++iter) {
    base::DictionaryValue* metric_details = new base::DictionaryValue();
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
scoped_ptr<base::DictionaryValue> GetMetricCategory(MetricCategory category) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
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
scoped_ptr<base::ListValue> GetEventTypesForCategory(EventCategory category) {
  scoped_ptr<base::ListValue> value(new base::ListValue());
  std::set<EventType> event_set = GetEventSetForCategory(category);
  for (std::set<EventType>::const_iterator iter = event_set.begin();
       iter != event_set.end(); ++iter) {
    base::DictionaryValue* event_details = new base::DictionaryValue();
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
scoped_ptr<base::DictionaryValue> GetEventCategory(EventCategory category) {
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
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
void DoGetActiveIntervals(base::ListValue* results,
                          const base::Time& start,
                          const base::Time& end) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  if (db == NULL)
    return;

  std::vector<TimeRange> intervals = db->GetActiveIntervals(start, end);

  for (std::vector<TimeRange>::iterator it = intervals.begin();
       it != intervals.end(); ++it) {
    base::DictionaryValue* interval_value = new base::DictionaryValue();
    interval_value->SetDouble("start", it->start.ToJsTime());
    interval_value->SetDouble("end", it->end.ToJsTime());
    results->Append(interval_value);
  }
}

// Queries the PerformanceMonitor database for events of type |event_type|
// between |start| and |end| times, creates a new event with localized keys
// for display, and appends the results to |results|.
void DoGetEvents(base::ListValue* results,
                 const std::set<EventType>& event_types,
                 const base::Time& start,
                 const base::Time& end) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  if (db == NULL)
    return;

  for (std::set<EventType>::const_iterator iter = event_types.begin();
       iter != event_types.end(); ++iter) {
    base::DictionaryValue* event_results = new base::DictionaryValue();
    event_results->SetInteger("eventId", static_cast<int>(*iter));
    base::ListValue* events = new base::ListValue();
    event_results->Set("events", events);
    results->Append(event_results);

    Database::EventVector event_vector = db->GetEvents(*iter, start, end);

    for (Database::EventVector::iterator event = event_vector.begin();
         event != event_vector.end(); ++event) {
      base::DictionaryValue* localized_event = new base::DictionaryValue();

      for (base::DictionaryValue::Iterator data(*(*event)->data());
           !data.IsAtEnd();
           data.Advance()) {
        base::Value* value = NULL;

        // The property 'eventType' is set in HandleGetEvents as part of the
        // entire result set, so we don't need to include this here in the
        // event.
        if (data.key() == "eventType") {
          continue;
        } else if (data.key() == "time") {
          // The property 'time' is also used computationally, but must be
          // converted to JS-style time.
          double time = 0.0;
          if (!data.value().GetAsDouble(&time)) {
            LOG(ERROR) << "Failed to get 'time' field from event.";
            continue;
          }
          value = new base::FundamentalValue(
              base::Time::FromInternalValue(static_cast<int64>(time))
                  .ToJsTime());
        } else {
          // All other values are user-facing, so we create a new value for
          // localized display.
          base::DictionaryValue* localized_value = new base::DictionaryValue();
          localized_value->SetString(
              "label",
              GetLocalizedStringFromEventProperty(data.key()));
          localized_value->SetWithoutPathExpansion("value",
                                                   data.value().DeepCopy());
          value = localized_value;
        }

        localized_event->SetWithoutPathExpansion(data.key(), value);
      }
      events->Append(localized_event);
    }
  }
}

// Populates results with a dictionary for each metric requested. The dictionary
// includes a metric id, the maximum value for the metric, and a list of lists
// of metric points, with each sublist containing the aggregated data for an
// interval for which PerformanceMonitor was active. This will also convert
// time to JS-style time.
void DoGetMetrics(base::ListValue* results,
                  const std::set<MetricType>& metric_types,
                  const base::Time& start,
                  const base::Time& end,
                  const base::TimeDelta& resolution,
                  AggregationMethod aggregation_method) {
  Database* db = PerformanceMonitor::GetInstance()->database();
  if (db == NULL)
    return;

  std::vector<TimeRange> intervals = db->GetActiveIntervals(start, end);

  // For each metric type, populate a new dictionary and append it to results.
  for (std::set<MetricType>::const_iterator metric_type = metric_types.begin();
       metric_type != metric_types.end(); ++metric_type) {
    double conversion_factor =
        GetConversionFactor(*GetUnitDetails(GetUnitForMetricType(*metric_type)),
                            *GetUnitDetails(GetUnitForMetricCategory(
                                GetCategoryForMetric(*metric_type))));

    base::DictionaryValue* metric_set = new base::DictionaryValue();
    metric_set->SetInteger("metricId", static_cast<int>(*metric_type));
    metric_set->SetDouble(
        "maxValue",
        db->GetMaxStatsForActivityAndMetric(*metric_type) * conversion_factor);

    // Retrieve all metrics in the database, and aggregate them into a series
    // of points for each active interval.
    scoped_ptr<Database::MetricVector> metric_vector =
        db->GetStatsForActivityAndMetric(*metric_type, start, end);

    scoped_ptr<VectorOfMetricVectors> aggregated_metrics =
        AggregateMetric(*metric_type,
                        metric_vector.get(),
                        start,
                        intervals,
                        resolution,
                        aggregation_method);

    // The JS-side expects a list to be present, even if there are no metrics.
    if (!aggregated_metrics) {
      metric_set->Set("metrics", new base::ListValue());
      results->Append(metric_set);
      continue;
    }

    base::ListValue* metric_points_by_interval = new base::ListValue();

    // For each metric point, record it in the expected format for the JS-side
    // (a dictionary of time and value, with time as a JS-style time), and
    // convert the values to be display-friendly.
    for (VectorOfMetricVectors::const_iterator metric_series =
             aggregated_metrics->begin();
         metric_series != aggregated_metrics->end(); ++metric_series) {
      base::ListValue* series_value = new base::ListValue();
      for (Database::MetricVector::const_iterator metric_point =
               metric_series->begin();
           metric_point != metric_series->end(); ++metric_point) {
        base::DictionaryValue* point_value = new base::DictionaryValue();
        point_value->SetDouble("time", metric_point->time.ToJsTime());
        point_value->SetDouble("value",
                               metric_point->value * conversion_factor);
        series_value->Append(point_value);
      }
      metric_points_by_interval->Append(series_value);
    }

    metric_set->Set("metrics", metric_points_by_interval);
    results->Append(metric_set);
  }
}

}  // namespace

PerformanceMonitorHandler::PerformanceMonitorHandler() {
}

PerformanceMonitorHandler::~PerformanceMonitorHandler() {}

void PerformanceMonitorHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getActiveIntervals",
      base::Bind(&PerformanceMonitorHandler::HandleGetActiveIntervals,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getFlagEnabled",
      base::Bind(&PerformanceMonitorHandler::HandleGetFlagEnabled,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getAggregationTypes",
      base::Bind(&PerformanceMonitorHandler::HandleGetAggregationTypes,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEventTypes",
      base::Bind(&PerformanceMonitorHandler::HandleGetEventTypes,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getEvents",
      base::Bind(&PerformanceMonitorHandler::HandleGetEvents,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getMetricTypes",
      base::Bind(&PerformanceMonitorHandler::HandleGetMetricTypes,
                 AsWeakPtr()));
  web_ui()->RegisterMessageCallback(
      "getMetrics",
      base::Bind(&PerformanceMonitorHandler::HandleGetMetrics,
                 AsWeakPtr()));
}

void PerformanceMonitorHandler::ReturnResults(const std::string& function,
                                 const base::Value* results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  web_ui()->CallJavascriptFunction(function, *results);
}

void PerformanceMonitorHandler::HandleGetActiveIntervals(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(2u, args->GetSize());
  double double_time = 0.0;
  CHECK(args->GetDouble(0, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(1, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  base::ListValue* results = new base::ListValue();
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetActiveIntervals, results, start, end),
      base::Bind(&PerformanceMonitorHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getActiveIntervalsCallback",
                 base::Owned(results)));
}

void PerformanceMonitorHandler::HandleGetFlagEnabled(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(0u, args->GetSize());
  base::FundamentalValue value(
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPerformanceMonitorGathering));
  ReturnResults("PerformanceMonitor.getFlagEnabledCallback", &value);
}

void PerformanceMonitorHandler::HandleGetAggregationTypes(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(0u, args->GetSize());
  base::ListValue results;
  for (int i = 0; i < AGGREGATION_METHOD_NUMBER_OF_METHODS; ++i) {
    results.Append(
        GetAggregationMethod(static_cast<AggregationMethod>(i)).release());
  }

  ReturnResults(
      "PerformanceMonitor.getAggregationTypesCallback", &results);
}

void PerformanceMonitorHandler::HandleGetEventTypes(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(0u, args->GetSize());
  base::ListValue results;
  for (int i = 0; i < EVENT_CATEGORY_NUMBER_OF_CATEGORIES; ++i)
    results.Append(GetEventCategory(static_cast<EventCategory>(i)).release());

  ReturnResults("PerformanceMonitor.getEventTypesCallback", &results);
}

void PerformanceMonitorHandler::HandleGetEvents(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(3u, args->GetSize());

  const base::ListValue* event_type_list;
  CHECK(args->GetList(0, &event_type_list));
  std::set<EventType> event_types;
  for (base::ListValue::const_iterator iter = event_type_list->begin();
       iter != event_type_list->end(); ++iter) {
    double event_type_double = 0.0;
    CHECK((*iter)->GetAsDouble(&event_type_double));
    CHECK(event_type_double < EVENT_NUMBER_OF_EVENTS &&
        event_type_double > EVENT_UNDEFINED);
    event_types.insert(
        static_cast<EventType>(static_cast<int>(event_type_double)));
  }

  double double_time = 0.0;
  CHECK(args->GetDouble(1, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(2, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  base::ListValue* results = new base::ListValue();
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetEvents, results, event_types, start, end),
      base::Bind(&PerformanceMonitorHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getEventsCallback",
                 base::Owned(results)));
}

void PerformanceMonitorHandler::HandleGetMetricTypes(
    const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(0u, args->GetSize());
  base::ListValue results;
  for (int i = 0; i < METRIC_CATEGORY_NUMBER_OF_CATEGORIES; ++i)
    results.Append(GetMetricCategory(static_cast<MetricCategory>(i)).release());

  ReturnResults("PerformanceMonitor.getMetricTypesCallback", &results);
}

void PerformanceMonitorHandler::HandleGetMetrics(const base::ListValue* args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CHECK_EQ(5u, args->GetSize());

  const base::ListValue* metric_type_list;
  CHECK(args->GetList(0, &metric_type_list));
  std::set<MetricType> metric_types;
  for (base::ListValue::const_iterator iter = metric_type_list->begin();
       iter != metric_type_list->end(); ++iter) {
    double metric_type_double = 0.0;
    CHECK((*iter)->GetAsDouble(&metric_type_double));
    CHECK(metric_type_double < METRIC_NUMBER_OF_METRICS &&
          metric_type_double > METRIC_UNDEFINED);
    metric_types.insert(
        static_cast<MetricType>(static_cast<int>(metric_type_double)));
  }

  double time_double = 0.0;
  CHECK(args->GetDouble(1, &time_double));
  base::Time start = base::Time::FromJsTime(time_double);
  CHECK(args->GetDouble(2, &time_double));
  base::Time end = base::Time::FromJsTime(time_double);

  double resolution_in_milliseconds = 0.0;
  CHECK(args->GetDouble(3, &resolution_in_milliseconds));
  base::TimeDelta resolution =
      base::TimeDelta::FromMilliseconds(resolution_in_milliseconds);

  double aggregation_double = 0.0;
  CHECK(args->GetDouble(4, &aggregation_double));
  CHECK(aggregation_double < AGGREGATION_METHOD_NUMBER_OF_METHODS &&
        aggregation_double >= 0);
  AggregationMethod aggregation_method =
      static_cast<AggregationMethod>(static_cast<int>(aggregation_double));

  base::ListValue* results = new base::ListValue();
  util::PostTaskToDatabaseThreadAndReply(
      FROM_HERE,
      base::Bind(&DoGetMetrics, results, metric_types,
                 start, end, resolution, aggregation_method),
      base::Bind(&PerformanceMonitorHandler::ReturnResults, AsWeakPtr(),
                 "PerformanceMonitor.getMetricsCallback",
                 base::Owned(results)));
}

}  // namespace performance_monitor
