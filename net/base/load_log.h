// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_LOAD_LOG_H_
#define NET_BASE_LOAD_LOG_H_

#include <string>
#include <vector>

#include "base/ref_counted.h"
#include "base/time.h"

namespace net {

// LoadLog stores information associated with an individual request. This
// includes event traces (used to build up profiling information), error
// return codes from network modules, and arbitrary text messages.
//
// Note that LoadLog is NOT THREADSAFE, however it is RefCountedThreadSafe so
// that it can be AddRef() / Release() across threads.
class LoadLog : public base::RefCountedThreadSafe<LoadLog> {
 public:
   // TODO(eroman): Really, EventType and EventPhase should be
   // Event::Type and Event::Phase, to be consisent with Entry.
   // But there lots of consumers to change!
  enum EventType {
#define EVENT_TYPE(label) TYPE_ ## label,
#include "net/base/load_log_event_type_list.h"
#undef EVENT_TYPE
  };

  // The 'phase' of an event trace (whether it marks the beginning or end
  // of an event.).
  enum EventPhase {
    PHASE_NONE,
    PHASE_BEGIN,
    // TODO(eroman): DEPRECATED: Use TYPE_STRING_LITERAL instead.
    PHASE_END,
  };

  struct Event {
    Event(EventType type, EventPhase phase) : type(type), phase(phase) {}
    Event() {}

    EventType type;
    EventPhase phase;
  };

  struct Entry {
    enum Type {
      // This entry describes an event trace.
      TYPE_EVENT,

      // This entry describes a network error code that was returned.
      TYPE_ERROR_CODE,

      // This entry is a free-form std::string.
      TYPE_STRING,

      // This entry is a C-string literal.
      TYPE_STRING_LITERAL,
    };

    Entry(base::TimeTicks time, int error_code)
        : type(TYPE_ERROR_CODE), time(time), error_code(error_code) {
    }

    Entry(base::TimeTicks time, const Event& event)
        : type(TYPE_EVENT), time(time), event(event) {
    }

    Entry(base::TimeTicks time, const std::string& string)
        : type(TYPE_STRING), time(time), string(string) {
    }

    Entry(base::TimeTicks time, const char* literal)
        : type(TYPE_STRING_LITERAL), time(time), literal(literal) {
    }

    Type type;
    base::TimeTicks time;

    // The following is basically a union, only one of them should be
    // used depending on what |type| is.
    Event event;          // valid when (type == TYPE_EVENT).
    int error_code;       // valid when (type == TYPE_ERROR_CODE).
    std::string string;   // valid when (type == TYPE_STRING).
    const char* literal;  // valid when (type == TYPE_STRING_LITERAL).
  };

  // Ordered set of entries that were logged.
  // TODO(eroman): use a StackVector or array to avoid allocations.
  typedef std::vector<Entry> EntryList;

  // Value for max_num_entries to indicate the LoadLog has no size limit.
  static const size_t kUnbounded = static_cast<size_t>(-1);

  // Creates a log, which can hold up to |max_num_entries| entries.
  // If |max_num_entries| is |kUnbounded|, then the log can grow arbitrarily
  // large.
  //
  // If entries are dropped because the log has grown too large, the final entry
  // will be overwritten.
  explicit LoadLog(size_t max_num_entries);

  // --------------------------------------------------------------------------

  // The public interface for adding events to the log are static methods.
  // This makes it easier to deal with optionally NULL LoadLog.

  // Adds an instantaneous event to the log.
  // TODO(eroman): DEPRECATED: use AddStringLiteral() instead.
  static void AddEvent(LoadLog* log, EventType event_type) {
    if (log)
      log->Add(Entry(base::TimeTicks::Now(), Event(event_type, PHASE_NONE)));
  }

  // Adds the start of an event to the log. Presumably this event type measures
  // a time duration, and will be matched by a call to EndEvent(event_type).
  static void BeginEvent(LoadLog* log, EventType event_type) {
    if (log)
      log->Add(Entry(base::TimeTicks::Now(), Event(event_type, PHASE_BEGIN)));
  }

  // Adds the end of an event to the log. Presumably this event type measures
  // a time duration, and we are matching an earlier call to
  // BeginEvent(event_type).
  static void EndEvent(LoadLog* log, EventType event_type) {
    if (log)
      log->Add(Entry(base::TimeTicks::Now(), Event(event_type, PHASE_END)));
  }

  // |literal| should be a string literal (i.e. lives in static storage).
  static void AddStringLiteral(LoadLog* log, const char* literal) {
    if (log)
      log->Add(Entry(base::TimeTicks::Now(), literal));
  }

  static void AddString(LoadLog* log, const std::string& string) {
    if (log)
      log->Add(Entry(base::TimeTicks::Now(), string));
  }

  // --------------------------------------------------------------------------

  // Returns the list of all entries in the log.
  const EntryList& entries() const {
    return entries_;
  }

  // Returns the number of entries that were dropped from the log because the
  // maximum size had been reached.
  size_t num_entries_truncated() const {
    return num_entries_truncated_;
  }

  // Returns the bound on the size of the log.
  size_t max_num_entries() const {
    return max_num_entries_;
  }

  // Returns a C-String symbolic name for |event|.
  static const char* EventTypeToString(EventType event_type);

  void Add(const Entry& entry);

  // Copies all entries from |log|, appending it to the end of |this|.
  void Append(const LoadLog* log);

 private:
  friend class base::RefCountedThreadSafe<LoadLog>;

  ~LoadLog() {}

  EntryList entries_;
  size_t num_entries_truncated_;
  size_t max_num_entries_;;
};

}  // namespace net

#endif  // NET_BASE_LOAD_LOG_H_
