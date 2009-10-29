// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_H_
#define NET_BASE_LOAD_LOG_H_

#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"

namespace net {

// LoadLog stores profiling information on where time was spent while servicing
// a request (waiting in queues, resolving hosts, resolving proxy, etc...).
//
// Note that LoadLog is NOT THREADSAFE, however it is RefCountedThreadSafe so
// that it can be AddRef() / Release() across threads.
class LoadLog : public base::RefCountedThreadSafe<LoadLog> {
 public:

  enum EventType {
#define EVENT_TYPE(label) TYPE_ ## label,
#include "net/base/load_log_event_type_list.h"
#undef EVENT_TYPE
  };

  // Whether this is the start/end of an event. Or in the case of EventTypes
  // that are "instantaneous", kNone.
  enum EventPhase {
    PHASE_NONE,
    PHASE_BEGIN,
    PHASE_END,
  };

  // A logged event. Note that "phase" means if this is the start/end of a
  // particular event type (in order to record a timestamp for both endpoints).
  struct Event {
    Event(base::TimeTicks time,
          EventType type,
          EventPhase phase)
        : time(time), type(type), phase(phase) {
    }

    base::TimeTicks time;
    EventType type;
    EventPhase phase;
  };

  // The maximum size of |events_|.
  enum { kMaxNumEntries = 40 };

  // Ordered set of events that were logged.
  // TODO(eroman): use a StackVector or array to avoid allocations.
  typedef std::vector<Event> EventList;

  // Create a log, which can hold up to |kMaxNumEntries| Events.
  //
  // If events are dropped because the log has grown too large, the final
  // entry will be of type kLogTruncated.
  LoadLog();

  // --------------------------------------------------------------------------

  // The public interface for adding events to the log are static methods.
  // This makes it easier to deal with optionally NULL LoadLog.

  // Adds an instantaneous event to the log.
  static void AddEvent(LoadLog* log, EventType event) {
    if (log)
      log->Add(base::TimeTicks::Now(), event, PHASE_NONE);
  }

  // Adds the start of an event to the log. Presumably this event type measures
  // a time duration, and will be matched by a call to EndEvent(event).
  static void BeginEvent(LoadLog* log, EventType event) {
    if (log)
      log->Add(base::TimeTicks::Now(), event, PHASE_BEGIN);
  }

  // Adds the end of an event to the log. Presumably this event type measures
  // a time duration, and we are matching an earlier call to BeginEvent(event).
  static void EndEvent(LoadLog* log, EventType event) {
    if (log)
      log->Add(base::TimeTicks::Now(), event, PHASE_END);
  }

  // --------------------------------------------------------------------------

  // Returns the list of all events in the log.
  const EventList& events() const {
    return events_;
  }

  // Returns a C-String symbolic name for |event|.
  static const char* EventTypeToString(EventType event);

  void Add(const Event& event);

  void Add(base::TimeTicks t, EventType event, EventPhase phase) {
    Add(Event(t, event, phase));
  }

  // Copies all events from |log|, appending it to the end of |this|.
  void Append(const LoadLog* log);

 private:
  EventList events_;
};

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_H_
