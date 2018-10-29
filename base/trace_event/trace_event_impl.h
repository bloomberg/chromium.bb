// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
#define BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/atomicops.h"
#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_util.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_arguments.h"
#include "base/trace_event/trace_event_memory_overhead.h"
#include "build/build_config.h"

namespace base {
namespace trace_event {

typedef base::Callback<bool(const char* arg_name)> ArgumentNameFilterPredicate;

typedef base::Callback<bool(const char* category_group_name,
                            const char* event_name,
                            ArgumentNameFilterPredicate*)>
    ArgumentFilterPredicate;

struct TraceEventHandle {
  uint32_t chunk_seq;
  // These numbers of bits must be kept consistent with
  // TraceBufferChunk::kMaxTrunkIndex and
  // TraceBufferChunk::kTraceBufferChunkSize (in trace_buffer.h).
  unsigned chunk_index : 26;
  unsigned event_index : 6;
};

class BASE_EXPORT TraceEvent {
 public:
  // TODO(898794): Remove once all users have been updated.
  using TraceValue = base::trace_event::TraceValue;

  TraceEvent();
  ~TraceEvent();

  // Allow move operations.
  TraceEvent(TraceEvent&&) noexcept;
  TraceEvent& operator=(TraceEvent&&) noexcept;

  TraceEvent(int thread_id,
             TimeTicks timestamp,
             ThreadTicks thread_timestamp,
             char phase,
             const unsigned char* category_group_enabled,
             const char* name,
             const char* scope,
             unsigned long long id,
             unsigned long long bind_id,
             TraceArguments* args,
             unsigned int flags);

  void Reset();

  void UpdateDuration(const TimeTicks& now, const ThreadTicks& thread_now);

  void EstimateTraceMemoryOverhead(TraceEventMemoryOverhead* overhead);

  // Serialize event data to JSON
  void AppendAsJSON(
      std::string* out,
      const ArgumentFilterPredicate& argument_filter_predicate) const;
  void AppendPrettyPrinted(std::ostringstream* out) const;

  // TODO(898794): Remove once caller has been updated.
  static void AppendValueAsJSON(unsigned char type,
                                TraceValue value,
                                std::string* out) {
    value.AppendAsJSON(type, out);
  }

  TimeTicks timestamp() const { return timestamp_; }
  ThreadTicks thread_timestamp() const { return thread_timestamp_; }
  char phase() const { return phase_; }
  int thread_id() const { return thread_id_; }
  TimeDelta duration() const { return duration_; }
  TimeDelta thread_duration() const { return thread_duration_; }
  const char* scope() const { return scope_; }
  unsigned long long id() const { return id_; }
  unsigned int flags() const { return flags_; }
  unsigned long long bind_id() const { return bind_id_; }
  // Exposed for unittesting:

  const StringStorage& parameter_copy_storage() const {
    return parameter_copy_storage_;
  }

  const unsigned char* category_group_enabled() const {
    return category_group_enabled_;
  }

  const char* name() const { return name_; }

  size_t arg_size() const { return args_.size(); }
  unsigned char arg_type(size_t index) const { return args_.types()[index]; }
  const char* arg_name(size_t index) const { return args_.names()[index]; }
  const TraceValue& arg_value(size_t index) const {
    return args_.values()[index];
  }

  const ConvertableToTraceFormat* arg_convertible_value(size_t index) const {
    return (arg_type(index) == TRACE_VALUE_TYPE_CONVERTABLE)
               ? arg_value(index).as_convertable
               : nullptr;
  }

#if defined(OS_ANDROID)
  void SendToATrace();
#endif

 private:
  // Note: these are ordered by size (largest first) for optimal packing.
  TimeTicks timestamp_;
  ThreadTicks thread_timestamp_;
  TimeDelta duration_;
  TimeDelta thread_duration_;
  // scope_ and id_ can be used to store phase-specific data.
  const char* scope_;
  unsigned long long id_;
  const unsigned char* category_group_enabled_;
  const char* name_;
  StringStorage parameter_copy_storage_;
  TraceArguments args_;
  // Depending on TRACE_EVENT_FLAG_HAS_PROCESS_ID the event will have either:
  //  tid: thread_id_, pid: current_process_id (default case).
  //  tid: -1, pid: process_id_ (when flags_ & TRACE_EVENT_FLAG_HAS_PROCESS_ID).
  union {
    int thread_id_;
    int process_id_;
  };
  unsigned int flags_;
  unsigned long long bind_id_;
  char phase_;

  DISALLOW_COPY_AND_ASSIGN(TraceEvent);
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_TRACE_EVENT_IMPL_H_
