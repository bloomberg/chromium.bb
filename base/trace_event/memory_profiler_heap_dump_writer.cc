// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_profiler_heap_dump_writer.h"

#include <numeric>

#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_profiler_allocation_register.h"
#include "base/trace_event/trace_event_argument.h"

namespace base {
namespace trace_event {

namespace {

template <typename T>
bool PairSizeGt(const std::pair<T, size_t>& lhs,
                const std::pair<T, size_t>& rhs) {
  return lhs.second > rhs.second;
}

template <typename T>
size_t PairSizeAdd(size_t acc, const std::pair<T, size_t>& rhs) {
  return acc + rhs.second;
}

}  // namespace

HeapDumpWriter::HeapDumpWriter(StackFrameDeduplicator* stack_frame_deduplicator)
    : traced_value_(new TracedValue()),
      stack_frame_deduplicator_(stack_frame_deduplicator) {}

HeapDumpWriter::~HeapDumpWriter() {}

void HeapDumpWriter::InsertAllocation(const AllocationContext& context,
                                      size_t size) {
  bytes_by_backtrace_[context.backtrace] += size;
}

scoped_refptr<TracedValue> HeapDumpWriter::WriteHeapDump() {
  // Sort the backtraces by size in descending order.
  std::vector<std::pair<Backtrace, size_t>> sorted_by_backtrace;

  std::copy(bytes_by_backtrace_.begin(), bytes_by_backtrace_.end(),
            std::back_inserter(sorted_by_backtrace));
  std::sort(sorted_by_backtrace.begin(), sorted_by_backtrace.end(),
            PairSizeGt<Backtrace>);

  traced_value_->BeginArray("entries");

  // The global size, no column specified.
  {
    size_t total_size =
        std::accumulate(sorted_by_backtrace.begin(), sorted_by_backtrace.end(),
                        size_t(0), PairSizeAdd<Backtrace>);
    traced_value_->BeginDictionary();
    WriteSize(total_size);
    traced_value_->EndDictionary();
  }

  // Size per backtrace.
  for (auto it = sorted_by_backtrace.begin();
       it != sorted_by_backtrace.end(); it++) {
    traced_value_->BeginDictionary();
    // Insert a forward reference to the backtrace that will be written to the
    // |stackFrames| dictionary later on.
    WriteStackFrameIndex(stack_frame_deduplicator_->Insert(it->first));
    WriteSize(it->second);
    traced_value_->EndDictionary();
  }

  traced_value_->EndArray();  // "entries"

  return traced_value_;
}

void HeapDumpWriter::WriteStackFrameIndex(int index) {
  if (index == -1) {
    // An empty backtrace (which will have index -1) is represented by the empty
    // string, because there is no leaf frame to reference in |stackFrames|.
    traced_value_->SetString("bt", "");
  } else {
    // Format index of the leaf frame as a string, because |stackFrames| is a
    // dictionary, not an array.
    SStringPrintf(&buffer_, "%i", index);
    traced_value_->SetString("bt", buffer_);
  }
}

void HeapDumpWriter::WriteSize(size_t size) {
  // Format size as hexadecimal string into |buffer_|.
  SStringPrintf(&buffer_, "%" PRIx64, static_cast<uint64_t>(size));
  traced_value_->SetString("size", buffer_);
}

}  // namespace trace_event
}  // namespace base
