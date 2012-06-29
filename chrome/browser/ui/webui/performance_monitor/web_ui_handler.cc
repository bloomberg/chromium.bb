// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/performance_monitor/web_ui_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/values.h"
#include "chrome/browser/performance_monitor/database.h"
#include "chrome/browser/performance_monitor/performance_monitor.h"
#include "chrome/browser/performance_monitor/performance_monitor_util.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"

namespace performance_monitor {
namespace {

void DoGetActiveIntervals(ListValue* results,
                          const base::Time& start, const base::Time& end) {
  std::vector<TimeRange> intervals =
      PerformanceMonitor::GetInstance()->database()->GetActiveIntervals(
          start, end);
  for (std::vector<TimeRange>::iterator it = intervals.begin();
       it != intervals.end(); ++it) {
    DictionaryValue* interval_value = new DictionaryValue();
    interval_value->SetDouble("start", it->start.ToDoubleT());
    interval_value->SetDouble("end", it->end.ToDoubleT());
    results->Append(interval_value);
  }
}

// TODO(mtytel): Add an internationalized longDescription field to each event.
void DoGetEvents(ListValue* results, EventType event,
                 const base::Time& start, const base::Time& end) {
  std::vector<linked_ptr<Event> > events =
      PerformanceMonitor::GetInstance()->database()->GetEvents(
          event, start, end);
  for (std::vector<linked_ptr<Event> >::iterator it = events.begin();
       it != events.end(); ++it) {
    results->Append((*it)->data());
  }
}

bool PostTaskToDatabaseAndReply(const base::Closure& request,
                                const base::Closure& reply) {
  base::SequencedWorkerPool* pool = content::BrowserThread::GetBlockingPool();
  base::SequencedWorkerPool::SequenceToken token = pool->GetSequenceToken();
  return pool->GetSequencedTaskRunner(token)->PostTaskAndReply(
      FROM_HERE, request, reply);
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
}

void WebUIHandler::ReturnResults(const std::string& function,
                                 const ListValue* results) {
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
  PostTaskToDatabaseAndReply(
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
  int event = 0;
  CHECK(args->GetInteger(0, &event));
  double double_time = 0.0;
  CHECK(args->GetDouble(1, &double_time));
  base::Time start = base::Time::FromJsTime(double_time);
  CHECK(args->GetDouble(2, &double_time));
  base::Time end = base::Time::FromJsTime(double_time);

  ListValue* results = new ListValue();
  PostTaskToDatabaseAndReply(
      base::Bind(&DoGetEvents, results, static_cast<EventType>(event),
                 start, end),
      base::Bind(&WebUIHandler::ReturnResults, AsWeakPtr(),
                 "performance_monitor.getEventsCallback",
                 base::Owned(results)));
}

}  // namespace performance_monitor
