// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/heap_profiler_stack_frame_deduplicator.h"

#include <inttypes.h>
#include <stddef.h>

#include <algorithm>
#include <string>
#include <utility>

#include "base/hash.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/heap_profiler_string_deduplicator.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "base/trace_event/trace_event_memory_overhead.h"

namespace base {
namespace trace_event {

namespace {

// Dumb hash function that nevertheless works surprisingly well and
// produces ~0 collisions on real backtraces.
size_t HashBacktrace(const StackFrame* begin, const StackFrame* end) {
  size_t hash = 0;
  for (; begin != end; begin++) {
    hash += reinterpret_cast<uintptr_t>(begin->value);
  }
  return hash;
}

}  // namespace

StackFrameDeduplicator::FrameNode::FrameNode(StackFrame frame,
                                             int parent_frame_index)
    : frame(frame), parent_frame_index(parent_frame_index) {}
StackFrameDeduplicator::FrameNode::FrameNode(const FrameNode& other) = default;
StackFrameDeduplicator::FrameNode::~FrameNode() {}

size_t StackFrameDeduplicator::FrameNode::EstimateMemoryUsage() const {
  return base::trace_event::EstimateMemoryUsage(children);
}

StackFrameDeduplicator::StackFrameDeduplicator(
    StringDeduplicator* string_deduplicator)
    : string_deduplicator_(string_deduplicator), last_exported_index_(0) {
  // Add implicit entry for id 0 (empty backtraces).
  frames_.push_back(FrameNode(StackFrame::FromTraceEventName(nullptr),
                              FrameNode::kInvalidFrameIndex));
}
StackFrameDeduplicator::~StackFrameDeduplicator() {}

bool StackFrameDeduplicator::Match(int frame_index,
                                   const StackFrame* begin_frame,
                                   const StackFrame* end_frame) const {
  // |frame_index| identifies the bottom frame, i.e. we need to walk
  // backtrace backwards.
  const StackFrame* current_frame = end_frame - 1;
  for (; current_frame >= begin_frame; --current_frame) {
    const FrameNode& node = frames_[frame_index];
    if (node.frame != *current_frame) {
      break;
    }

    frame_index = node.parent_frame_index;
    if (frame_index == FrameNode::kInvalidFrameIndex) {
      if (current_frame == begin_frame) {
        // We're at the top node and we matched all backtrace frames,
        // i.e. we successfully matched the backtrace.
        return true;
      }
      break;
    }
  }

  return false;
}

int StackFrameDeduplicator::Insert(const StackFrame* begin_frame,
                                   const StackFrame* end_frame) {
  if (begin_frame == end_frame) {
    // Empty backtraces are mapped to id 0.
    return 0;
  }

  size_t backtrace_hash = HashBacktrace(begin_frame, end_frame);

  // Check if we know about this backtrace.
  auto backtrace_it = backtrace_lookup_table_.find(backtrace_hash);
  if (backtrace_it != backtrace_lookup_table_.end()) {
    int backtrace_index = backtrace_it->second;
    if (Match(backtrace_index, begin_frame, end_frame)) {
      return backtrace_index;
    }
  }

  int frame_index = FrameNode::kInvalidFrameIndex;
  base::flat_map<StackFrame, int>* nodes = &roots_;

  // Loop through the frames, early out when a frame is null.
  for (const StackFrame* it = begin_frame; it != end_frame; it++) {
    StackFrame frame = *it;

    auto node = nodes->find(frame);
    if (node == nodes->end()) {
      // There is no tree node for this frame yet, create it. The parent node
      // is the node associated with the previous frame.
      FrameNode frame_node(frame, frame_index);

      // The new frame node will be appended, so its index is the current size
      // of the vector.
      frame_index = static_cast<int>(frames_.size());

      // Add the node to the trie so it will be found next time.
      nodes->insert(std::make_pair(frame, frame_index));

      // Append the node after modifying |nodes|, because the |frames_| vector
      // might need to resize, and this invalidates the |nodes| pointer.
      frames_.push_back(frame_node);
    } else {
      // A tree node for this frame exists. Look for the next one.
      frame_index = node->second;
    }

    nodes = &frames_[frame_index].children;
  }

  // Remember the backtrace.
  backtrace_lookup_table_[backtrace_hash] = frame_index;

  return frame_index;
}

void StackFrameDeduplicator::SerializeIncrementally(TracedValue* traced_value) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("memory-infra"),
               "StackFrameDeduplicator::SerializeIncrementally");
  std::string stringify_buffer;

  for (; last_exported_index_ < frames_.size(); ++last_exported_index_) {
    const auto& frame_node = frames_[last_exported_index_];
    traced_value->BeginDictionary();

    traced_value->SetInteger("id", last_exported_index_);

    int name_string_id = 0;
    const StackFrame& frame = frame_node.frame;
    switch (frame.type) {
      case StackFrame::Type::TRACE_EVENT_NAME:
        name_string_id =
            string_deduplicator_->Insert(static_cast<const char*>(frame.value));
        break;
      case StackFrame::Type::THREAD_NAME:
        SStringPrintf(&stringify_buffer,
                      "[Thread: %s]",
                      static_cast<const char*>(frame.value));
        name_string_id = string_deduplicator_->Insert(stringify_buffer);
        break;
      case StackFrame::Type::PROGRAM_COUNTER:
        SStringPrintf(&stringify_buffer,
                      "pc:%" PRIxPTR,
                      reinterpret_cast<uintptr_t>(frame.value));
        name_string_id = string_deduplicator_->Insert(stringify_buffer);
        break;
    }
    traced_value->SetInteger("name_sid", name_string_id);

    if (frame_node.parent_frame_index != FrameNode::kInvalidFrameIndex) {
      traced_value->SetInteger("parent", frame_node.parent_frame_index);
    }

    traced_value->EndDictionary();
  }
}

void StackFrameDeduplicator::EstimateTraceMemoryOverhead(
    TraceEventMemoryOverhead* overhead) {
  size_t memory_usage = EstimateMemoryUsage(frames_) +
                        EstimateMemoryUsage(roots_) +
                        EstimateMemoryUsage(backtrace_lookup_table_);
  overhead->Add(TraceEventMemoryOverhead::kHeapProfilerStackFrameDeduplicator,
                sizeof(StackFrameDeduplicator) + memory_usage);
}

}  // namespace trace_event
}  // namespace base
