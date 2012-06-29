// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_H_

#include "base/values.h"
#include "base/time.h"
#include "base/memory/scoped_ptr.h"

namespace performance_monitor {

// IMPORTANT: This is used as an indication of the event type within the
// performance monitor database; do not change the order! If you add new events
// to this list, place them above EVENT_NUMBER_OF_EVENTS and add a string to
// kEventTypeNames in the cc file.
enum EventType {
  EVENT_UNDEFINED,
  EVENT_EXTENSION_INSTALL,
  EVENT_EXTENSION_UNINSTALL,
  EVENT_EXTENSION_UPDATE,
  EVENT_EXTENSION_ENABLE,
  EVENT_EXTENSION_UNLOAD,
  EVENT_CHROME_UPDATE,
  EVENT_RENDERER_FREEZE,
  EVENT_RENDERER_CRASH,
  EVENT_OOM_CRASH,
  EVENT_UNCLEAN_SHUTDOWN,
  EVENT_NUMBER_OF_EVENTS
};

const char* EventTypeToString(EventType event_type);

// The wrapper class for the JSON-generated event classes for the performance
// monitor. This class is used so we can pass around events in a single class,
// rather than having a variety of different types (since JSON does not
// currently support inheritance). Since the class will occasionally need to
// be compared against other events, we construct it with type and time. Other
// information should not be needed commonly, and is stored in a JSON-generated
// DictionaryValue.
class Event {
 public:
  Event(const EventType& type,
        const base::Time& time,
        scoped_ptr<base::DictionaryValue> data);
  virtual ~Event();

  // Construct an event from the given DictionaryValue; takes ownership of
  // |data|.
  static scoped_ptr<Event> FromValue(scoped_ptr<base::DictionaryValue> data);

  // Accessors
  EventType type() const { return type_; }
  base::Time time() const { return time_; }
  base::DictionaryValue* data() const { return data_.get(); }

 private:

  // The type of the event.
  EventType type_;
  // The time at which the event was recorded.
  base::Time time_;
  // The full JSON-generated value representing the information of the event;
  // these data are described in chrome/browser/performance_monitor/events.json.
  scoped_ptr<base::DictionaryValue> data_;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_EVENT_H_
