// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/heap_profiler_allocation_context_tracker.h"

#include <algorithm>
#include <iterator>

#include "base/atomicops.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/heap_profiler_allocation_context.h"

namespace base {
namespace trace_event {

subtle::Atomic32 AllocationContextTracker::capture_enabled_ = 0;

namespace {

ThreadLocalStorage::StaticSlot g_tls_alloc_ctx_tracker = TLS_INITIALIZER;

// This function is added to the TLS slot to clean up the instance when the
// thread exits.
void DestructAllocationContextTracker(void* alloc_ctx_tracker) {
  delete static_cast<AllocationContextTracker*>(alloc_ctx_tracker);
}

}  // namespace

AllocationContextTracker::AllocationContextTracker() {}
AllocationContextTracker::~AllocationContextTracker() {}

// static
AllocationContextTracker* AllocationContextTracker::GetThreadLocalTracker() {
  auto tracker =
      static_cast<AllocationContextTracker*>(g_tls_alloc_ctx_tracker.Get());

  if (!tracker) {
    tracker = new AllocationContextTracker();
    g_tls_alloc_ctx_tracker.Set(tracker);
  }

  return tracker;
}

// static
void AllocationContextTracker::SetCaptureEnabled(bool enabled) {
  // When enabling capturing, also initialize the TLS slot. This does not create
  // a TLS instance yet.
  if (enabled && !g_tls_alloc_ctx_tracker.initialized())
    g_tls_alloc_ctx_tracker.Initialize(DestructAllocationContextTracker);

  // Release ordering ensures that when a thread observes |capture_enabled_| to
  // be true through an acquire load, the TLS slot has been initialized.
  subtle::Release_Store(&capture_enabled_, enabled);
}

// static
void AllocationContextTracker::PushPseudoStackFrame(StackFrame frame) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();

  // Impose a limit on the height to verify that every push is popped, because
  // in practice the pseudo stack never grows higher than ~20 frames.
  DCHECK_LT(tracker->pseudo_stack_.size(), 128u);
  tracker->pseudo_stack_.push_back(frame);
}

// static
void AllocationContextTracker::PopPseudoStackFrame(StackFrame frame) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();

  // Guard for stack underflow. If tracing was started with a TRACE_EVENT in
  // scope, the frame was never pushed, so it is possible that pop is called
  // on an empty stack.
  if (tracker->pseudo_stack_.empty())
    return;

  // Assert that pushes and pops are nested correctly. This DCHECK can be
  // hit if some TRACE_EVENT macro is unbalanced (a TRACE_EVENT_END* call
  // without a corresponding TRACE_EVENT_BEGIN).
  DCHECK_EQ(frame, tracker->pseudo_stack_.back())
      << "Encountered an unmatched TRACE_EVENT_END";

  tracker->pseudo_stack_.pop_back();
}

// static
AllocationContext AllocationContextTracker::GetContextSnapshot() {
  AllocationContextTracker* tracker = GetThreadLocalTracker();
  AllocationContext ctx;

  // Fill the backtrace.
  {
    auto src = tracker->pseudo_stack_.begin();
    auto dst = std::begin(ctx.backtrace.frames);
    auto src_end = tracker->pseudo_stack_.end();
    auto dst_end = std::end(ctx.backtrace.frames);

    // Copy as much of the bottom of the pseudo stack into the backtrace as
    // possible.
    for (; src != src_end && dst != dst_end; src++, dst++)
      *dst = *src;

    // If there is room for more, fill the remaining slots with empty frames.
    std::fill(dst, dst_end, nullptr);
  }

  ctx.type_name = nullptr;

  return ctx;
}

}  // namespace trace_event
}  // namespace base
