// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/event.h"

#include "base/logging.h"

namespace performance_monitor {
namespace {

// Keep this array synced with EventTypes in the header file.
// TODO(mtytel): i18n.
const char* kEventTypeNames[] = {
  "Undefined",
  "Extension Installs",
  "Extension Uninstalls",
  "Extension Updates",
  "Extension Enables",
  "Extension Unloads",
  "Chrome Updates",
  "Renderer Freezes",
  "Renderer Crashes",
  "Out of Memory Crashes",
  "Unclean Shutdowns"
};
COMPILE_ASSERT(ARRAYSIZE_UNSAFE(kEventTypeNames) == EVENT_NUMBER_OF_EVENTS,
               event_names_incorrect_size);

}  // namespace

const char* EventTypeToString(EventType event_type) {
  DCHECK_GT(EVENT_NUMBER_OF_EVENTS, event_type);
  return kEventTypeNames[event_type];
}

Event::Event(const EventType& type,
             const base::Time& time,
             scoped_ptr<base::DictionaryValue> data)
    : type_(type), time_(time), data_(data.release()) {
}

Event::~Event() {
}

scoped_ptr<Event> Event::FromValue(scoped_ptr<base::DictionaryValue> data) {
  int type = 0;
  if (!data->GetInteger(std::string("eventType"), &type))
    return scoped_ptr<Event>();
  double time = 0.0;
  if (!data->GetDouble(std::string("time"), &time))
    return scoped_ptr<Event>();
  return scoped_ptr<Event>(new Event(static_cast<EventType>(type),
                                     base::Time::FromInternalValue((int64)time),
                                     data.Pass()));
}

}  // namespace performance_monitor
