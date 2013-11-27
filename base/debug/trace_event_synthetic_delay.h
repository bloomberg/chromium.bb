// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The synthetic delay framework makes it possible to dynamically inject
// arbitrary delays into into different parts of the codebase. This can be used,
// for instance, for testing various task scheduling algorithms.
//
// The delays are specified in terms of a target duration for a given block of
// code. If the code executes faster than the duration, the thread is made to
// sleep until the deadline is met.
//
// Code can be instrumented for delays with two sets of macros. First, for
// delays that should apply within a scope, use the following macro:
//
//   TRACE_EVENT_SYNTHETIC_DELAY("cc.LayerTreeHost.DrawAndSwap");
//
// For delaying operations that span multiple scopes, use:
//
//   TRACE_EVENT_SYNTHETIC_DELAY_ACTIVATE("cc.Scheduler.BeginMainFrame");
//   ...
//   TRACE_EVENT_SYNTHETIC_DELAY_APPLY("cc.Scheduler.BeginMainFrame");
//
// Here ACTIVATE establishes the start time for the delay and APPLY executes the
// delay based on the remaining time. ACTIVATE may be called one or multiple
// times before APPLY. Only the first call will have an effect. If ACTIVATE
// hasn't been called since the last call to APPLY, APPLY will be a no-op.
//
// Note that while the same delay can be applied in several threads
// simultaneously, a single delay operation cannot begin on one thread and end
// on another.

#ifndef BASE_DEBUG_TRACE_EVENT_SYNTHETIC_DELAY_H_
#define BASE_DEBUG_TRACE_EVENT_SYNTHETIC_DELAY_H_

#include "base/atomicops.h"
#include "base/debug/trace_event.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/time/time.h"

// Apply a named delay in the current scope.
#define TRACE_EVENT_SYNTHETIC_DELAY(name)                                     \
  static base::subtle::AtomicWord INTERNAL_TRACE_EVENT_UID(impl_ptr) = 0;     \
  trace_event_internal::ScopedSyntheticDelay INTERNAL_TRACE_EVENT_UID(delay)( \
      name, &INTERNAL_TRACE_EVENT_UID(impl_ptr));

// Activate a named delay, establishing its timing start point. May be called
// multiple times, but only the first call will have an effect.
#define TRACE_EVENT_SYNTHETIC_DELAY_ACTIVATE(name)                       \
  do {                                                                   \
    static base::subtle::AtomicWord impl_ptr = 0;                        \
    trace_event_internal::GetOrCreateDelay(name, &impl_ptr)->Activate(); \
  } while (false)

// Apply a named delay. If TRACE_EVENT_SYNTHETIC_DELAY_ACTIVATE was called for
// the same delay, that point in time is used as the delay start point. If not,
// this call will be a no-op.
#define TRACE_EVENT_SYNTHETIC_DELAY_APPLY(name)                       \
  do {                                                                \
    static base::subtle::AtomicWord impl_ptr = 0;                     \
    trace_event_internal::GetOrCreateDelay(name, &impl_ptr)->Apply(); \
  } while (false)

template <typename Type>
struct DefaultSingletonTraits;

namespace base {
namespace debug {

// Time source for computing delay durations. Used for testing.
class TRACE_EVENT_API_CLASS_EXPORT TraceEventSyntheticDelayClock {
 public:
  TraceEventSyntheticDelayClock();
  virtual ~TraceEventSyntheticDelayClock();
  virtual base::TimeTicks Now() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TraceEventSyntheticDelayClock);
};

// Single delay point instance.
class TRACE_EVENT_API_CLASS_EXPORT TraceEventSyntheticDelay {
 public:
  enum Mode {
    STATIC,      // Apply the configured delay every time.
    ONE_SHOT,    // Apply the configured delay just once.
    ALTERNATING  // Apply the configured delay every other time.
  };

  // Returns an existing named delay instance or creates a new one with |name|.
  static TraceEventSyntheticDelay* Lookup(const std::string& name);

  void SetTargetDuration(TimeDelta target_duration);
  void SetMode(Mode mode);
  void SetClock(TraceEventSyntheticDelayClock* clock);

  // Establish the timing start point for the delay. No-op if the start point
  // was already set.
  void Activate();

  // Execute the delay based on the current time and how long ago the start
  // point was established. If Activate wasn't called, this call will be a
  // no-op.
  void Apply();

 private:
  TraceEventSyntheticDelay();
  ~TraceEventSyntheticDelay();
  friend class TraceEventSyntheticDelayRegistry;

  void Initialize(const std::string& name,
                  TraceEventSyntheticDelayClock* clock);
  void ApplyDelay(base::TimeTicks end_time);

  struct ThreadState {
    ThreadState();

    base::TimeTicks start_time;
    unsigned trigger_count;
    int generation;
  };

  ThreadState* EnsureThreadState();

  Lock lock_;
  Mode mode_;
  std::string name_;
  int generation_;
  base::TimeDelta target_duration_;
  ThreadLocalPointer<ThreadState> thread_state_;
  TraceEventSyntheticDelayClock* clock_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventSyntheticDelay);
};

}  // namespace debug
}  // namespace base

namespace trace_event_internal {

// Helper class for scoped delays. Do not use directly.
class TRACE_EVENT_API_CLASS_EXPORT ScopedSyntheticDelay {
 public:
  explicit ScopedSyntheticDelay(const char* name,
                                base::subtle::AtomicWord* impl_ptr);
  ~ScopedSyntheticDelay();

 private:
  base::debug::TraceEventSyntheticDelay* delay_impl_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSyntheticDelay);
};

// Helper for registering delays. Do not use directly.
TRACE_EVENT_API_CLASS_EXPORT base::debug::TraceEventSyntheticDelay*
    GetOrCreateDelay(const char* name, base::subtle::AtomicWord* impl_ptr);

}  // namespace trace_event_internal

#endif /* BASE_DEBUG_TRACE_EVENT_SYNTHETIC_DELAY_H_ */
