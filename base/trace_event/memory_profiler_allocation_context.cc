// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_profiler_allocation_context.h"

#include <algorithm>

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
  auto tracker =
      static_cast<AllocationContextTracker*>(g_tls_alloc_ctx_tracker.Get());

  if (!tracker) {
    tracker = new AllocationContextTracker();
    g_tls_alloc_ctx_tracker.Set(tracker);
  }

  return tracker;
}

StackFrameDeduplicator::FrameNode::FrameNode(StackFrame frame,
                                             int parent_frame_index)
    : frame(frame), parent_frame_index(parent_frame_index) {}
StackFrameDeduplicator::FrameNode::~FrameNode() {}

StackFrameDeduplicator::StackFrameDeduplicator() {}
StackFrameDeduplicator::~StackFrameDeduplicator() {}

int StackFrameDeduplicator::Insert(const AllocationContext::Backtrace& bt) {
  int frame_index = -1;
  std::map<StackFrame, int>* nodes = &roots_;

  for (size_t i = 0; i < arraysize(bt.frames); i++) {
    if (!bt.frames[i])
      break;

    auto node = nodes->find(bt.frames[i]);
    if (node == nodes->end()) {
      // There is no tree node for this frame yet, create it. The parent node
      // is the node associated with the previous frame.
      FrameNode frame_node(bt.frames[i], frame_index);

      // The new frame node will be appended, so its index is the current size
      // of the vector.
      frame_index = static_cast<int>(frames_.size());

      // Add the node to the trie so it will be found next time.
      nodes->insert(std::make_pair(bt.frames[i], frame_index));

      // Append the node after modifying |children|, because the vector might
      // need to resize, and this invalidates the |children| pointer.
      frames_.push_back(frame_node);
    } else {
      // A tree node for this frame exists. Look for the next one.
      frame_index = node->second;
    }

    nodes = &frames_[frame_index].children;
  }

  return frame_index;
}

AllocationContextTracker::AllocationContextTracker() {}
AllocationContextTracker::~AllocationContextTracker() {}

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
  tracker->pseudo_stack_.push(frame);
}

// static
void AllocationContextTracker::PopPseudoStackFrame(StackFrame frame) {
  auto tracker = AllocationContextTracker::GetThreadLocalTracker();
  // Assert that pushes and pops are nested correctly. |top()| points past the
  // top of the stack, so |top() - 1| dereferences to the topmost frame.
  DCHECK_EQ(frame, *(tracker->pseudo_stack_.top() - 1));
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

// Returns a pointer past the end of the fixed-size array |array| of |T| of
// length |N|, identical to C++11 |std::end|.
template <typename T, int N>
T* End(T(&array)[N]) {
  return array + N;
}

// static
AllocationContext AllocationContextTracker::GetContextSnapshot() {
  AllocationContextTracker* tracker = GetThreadLocalTracker();
  AllocationContext ctx;

  // Fill the backtrace.
  {
    auto src = tracker->pseudo_stack_.bottom();
    auto dst = ctx.backtrace.frames;
    auto src_end = tracker->pseudo_stack_.top();
    auto dst_end = End(ctx.backtrace.frames);

    // Copy as much of the bottom of the pseudo stack into the backtrace as
    // possible.
    for (; src != src_end && dst != dst_end; src++, dst++)
      *dst = *src;

    // If there is room for more, fill the remaining slots with empty frames.
    std::fill(dst, dst_end, nullptr);
  }

  // Fill the context fields.
  {
    auto src = tracker->context_.begin();
    auto dst = ctx.fields;
    auto src_end = tracker->context_.end();
    auto dst_end = End(ctx.fields);

    // Copy as much (key, value) pairs as possible.
    for (; src != src_end && dst != dst_end; src++, dst++)
      *dst = *src;

    // If there is room for more, fill the remaining slots with nullptr keys.
    for (; dst != dst_end; dst++)
      dst->first = nullptr;
  }

  return ctx;
}

}  // namespace trace_event
}  // namespace base
