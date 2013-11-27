// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/trace_event_synthetic_delay.h"
#include "base/memory/singleton.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"

namespace {
const int kMaxSyntheticDelays = 32;
}  // namespace

namespace base {
namespace debug {

TraceEventSyntheticDelayClock::TraceEventSyntheticDelayClock() {}
TraceEventSyntheticDelayClock::~TraceEventSyntheticDelayClock() {}

class TraceEventSyntheticDelayRegistry : public TraceEventSyntheticDelayClock {
 public:
  static TraceEventSyntheticDelayRegistry* GetInstance();

  TraceEventSyntheticDelay* GetOrCreateDelay(const char* name);

  // TraceEventSyntheticDelayClock implementation.
  virtual base::TimeTicks Now() OVERRIDE;

 private:
  TraceEventSyntheticDelayRegistry();

  friend struct DefaultSingletonTraits<TraceEventSyntheticDelayRegistry>;

  Lock lock_;
  TraceEventSyntheticDelay delays_[kMaxSyntheticDelays];
  TraceEventSyntheticDelay dummy_delay_;
  base::subtle::Atomic32 delay_count_;

  DISALLOW_COPY_AND_ASSIGN(TraceEventSyntheticDelayRegistry);
};

TraceEventSyntheticDelay::TraceEventSyntheticDelay()
    : mode_(STATIC), generation_(0), clock_(NULL) {}

TraceEventSyntheticDelay::~TraceEventSyntheticDelay() {}

TraceEventSyntheticDelay* TraceEventSyntheticDelay::Lookup(
    const std::string& name) {
  return TraceEventSyntheticDelayRegistry::GetInstance()->GetOrCreateDelay(
      name.c_str());
}

void TraceEventSyntheticDelay::Initialize(
    const std::string& name,
    TraceEventSyntheticDelayClock* clock) {
  DCHECK(!thread_state_.Get());
  name_ = name;
  clock_ = clock;
}

void TraceEventSyntheticDelay::SetTargetDuration(
    base::TimeDelta target_duration) {
  AutoLock lock(lock_);
  target_duration_ = target_duration;
  generation_++;
}

void TraceEventSyntheticDelay::SetMode(Mode mode) {
  AutoLock lock(lock_);
  mode_ = mode;
  generation_++;
}

void TraceEventSyntheticDelay::SetClock(TraceEventSyntheticDelayClock* clock) {
  AutoLock lock(lock_);
  clock_ = clock;
  generation_++;
}

TraceEventSyntheticDelay::ThreadState::ThreadState()
    : trigger_count(0u), generation(0) {}

void TraceEventSyntheticDelay::Activate() {
  // Note that we check for a non-zero target duration without locking to keep
  // things quick for the common case when delays are disabled. Since the delay
  // calculation is done with a lock held, it will always be correct. The only
  // downside of this is that we may fail to apply some delays when the target
  // duration changes.
  ANNOTATE_BENIGN_RACE(&target_duration_, "Synthetic delay duration");
  if (!target_duration_.ToInternalValue())
    return;

  // Store the start time in a thread local struct so the same delay can be
  // applied to multiple threads simultaneously. Note that the structs are
  // leaked at program exit.
  ThreadState* thread_state = EnsureThreadState();
  if (!thread_state->start_time.ToInternalValue())
    thread_state->start_time = clock_->Now();
}

TraceEventSyntheticDelay::ThreadState*
TraceEventSyntheticDelay::EnsureThreadState() {
  ThreadState* thread_state = thread_state_.Get();
  if (!thread_state)
    thread_state_.Set((thread_state = new ThreadState()));
  return thread_state;
}

void TraceEventSyntheticDelay::Apply() {
  ANNOTATE_BENIGN_RACE(&target_duration_, "Synthetic delay duration");
  if (!target_duration_.ToInternalValue())
    return;

  ThreadState* thread_state = EnsureThreadState();
  if (!thread_state->start_time.ToInternalValue())
    return;
  base::TimeTicks now = clock_->Now();
  base::TimeTicks start_time = thread_state->start_time;
  base::TimeTicks end_time;
  thread_state->start_time = base::TimeTicks();

  {
    AutoLock lock(lock_);
    if (thread_state->generation != generation_) {
      thread_state->trigger_count = 0;
      thread_state->generation = generation_;
    }

    if (mode_ == ONE_SHOT && thread_state->trigger_count++)
      return;
    else if (mode_ == ALTERNATING && thread_state->trigger_count++ % 2)
      return;

    end_time = start_time + target_duration_;
    if (now >= end_time)
      return;
  }
  ApplyDelay(end_time);
}

void TraceEventSyntheticDelay::ApplyDelay(base::TimeTicks end_time) {
  TRACE_EVENT0("synthetic_delay", name_.c_str());
  while (clock_->Now() < end_time) {
    // Busy loop.
  }
}

TraceEventSyntheticDelayRegistry*
TraceEventSyntheticDelayRegistry::GetInstance() {
  return Singleton<
      TraceEventSyntheticDelayRegistry,
      LeakySingletonTraits<TraceEventSyntheticDelayRegistry> >::get();
}

TraceEventSyntheticDelayRegistry::TraceEventSyntheticDelayRegistry()
    : delay_count_(0) {}

TraceEventSyntheticDelay* TraceEventSyntheticDelayRegistry::GetOrCreateDelay(
    const char* name) {
  // Try to find an existing delay first without locking to make the common case
  // fast.
  int delay_count = base::subtle::Acquire_Load(&delay_count_);
  for (int i = 0; i < delay_count; ++i) {
    if (!strcmp(name, delays_[i].name_.c_str()))
      return &delays_[i];
  }

  AutoLock lock(lock_);
  delay_count = base::subtle::Acquire_Load(&delay_count_);
  for (int i = 0; i < delay_count; ++i) {
    if (!strcmp(name, delays_[i].name_.c_str()))
      return &delays_[i];
  }

  DCHECK(delay_count < kMaxSyntheticDelays)
      << "must increase kMaxSyntheticDelays";
  if (delay_count >= kMaxSyntheticDelays)
    return &dummy_delay_;

  delays_[delay_count].Initialize(std::string(name), this);
  base::subtle::Release_Store(&delay_count_, delay_count + 1);
  return &delays_[delay_count];
}

base::TimeTicks TraceEventSyntheticDelayRegistry::Now() {
  return base::TimeTicks::HighResNow();
}

}  // namespace debug
}  // namespace base

namespace trace_event_internal {

ScopedSyntheticDelay::ScopedSyntheticDelay(const char* name,
                                           base::subtle::AtomicWord* impl_ptr)
    : delay_impl_(GetOrCreateDelay(name, impl_ptr)) {
  delay_impl_->Activate();
}

ScopedSyntheticDelay::~ScopedSyntheticDelay() { delay_impl_->Apply(); }

base::debug::TraceEventSyntheticDelay* GetOrCreateDelay(
    const char* name,
    base::subtle::AtomicWord* impl_ptr) {
  base::debug::TraceEventSyntheticDelay* delay_impl =
      reinterpret_cast<base::debug::TraceEventSyntheticDelay*>(
          base::subtle::NoBarrier_Load(impl_ptr));
  if (!delay_impl) {
    delay_impl = base::debug::TraceEventSyntheticDelayRegistry::GetInstance()
                     ->GetOrCreateDelay(name);
    base::subtle::NoBarrier_Store(
        impl_ptr, reinterpret_cast<base::subtle::AtomicWord>(delay_impl));
  }
  return delay_impl;
}

}  // namespace trace_event_internal
