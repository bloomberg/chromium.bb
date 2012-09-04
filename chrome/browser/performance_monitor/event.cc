// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/event.h"

#include "base/logging.h"

namespace performance_monitor {

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
