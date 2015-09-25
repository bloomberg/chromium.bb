// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_profiler_allocation_context.h"

#include "base/threading/thread_local_storage.h"

namespace base {
namespace trace_event {

subtle::Atomic32 AllocationContextTracker::capture_enabled_ = 0;

namespace {
ThreadLocalStorage::StaticSlot g_tls_alloc_ctx_tracker = TLS_INITIALIZER;
}

AllocationStack::AllocationStack() {}
AllocationStack::~AllocationStack() {}

// This function is added to the TLS slot to clean up the instance when the
// thread exits.
void DestructAllocationContextTracker(void* alloc_ctx_tracker) {
  delete static_cast<AllocationContextTracker*>(alloc_ctx_tracker);
}

AllocationContextTracker* AllocationContextTracker::GetThreadLocalTracker() {
  AllocationContextTracker* tracker;

  if (g_tls_alloc_ctx_tracker.initialized()) {
    tracker =
        static_cast<AllocationContextTracker*>(g_tls_alloc_ctx_tracker.Get());
  } else {
    tracker = new AllocationContextTracker();
    g_tls_alloc_ctx_tracker.Initialize(DestructAllocationContextTracker);
    g_tls_alloc_ctx_tracker.Set(tracker);
  }

  return tracker;
}

AllocationContextTracker::AllocationContextTracker() {}
AllocationContextTracker::~AllocationContextTracker() {}

// static
void AllocationContextTracker::SetCaptureEnabled(bool enabled) {
  // There is no memory barrier here for performance reasons, a little lag is
  // not an issue.
  subtle::NoBarrier_Store(&capture_enabled_, enabled);
}

// static
void AllocationContextTracker::PushPseudoStackFrame(StackFrame frame) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  tracker->pseudo_stack_.push(frame);
}

// static
void AllocationContextTracker::PopPseudoStackFrame(StackFrame frame) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  DCHECK_EQ(frame, *tracker->pseudo_stack_.top());
  tracker->pseudo_stack_.pop();
}

// static
void AllocationContextTracker::SetContextField(const char* key,
                                               const char* value) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  tracker->context_[key] = value;
}

// static
void AllocationContextTracker::UnsetContextField(const char* key) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  tracker->context_.erase(key);
}

// static
AllocationStack* AllocationContextTracker::GetPseudoStackForTesting() {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  return &tracker->pseudo_stack_;
}

// static
AllocationContext AllocationContextTracker::GetContext() {
  // TODO(ruuda): Implement this in a follow-up CL.
  return AllocationContext();
}

}  // namespace trace_event
}  // namespace base
