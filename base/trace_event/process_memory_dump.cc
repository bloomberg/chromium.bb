// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include "base/trace_event/process_memory_totals.h"
#include "base/trace_event/trace_event_argument.h"

namespace base {
namespace trace_event {

ProcessMemoryDump::ProcessMemoryDump(
    const scoped_refptr<MemoryDumpSessionState>& session_state)
    : has_process_totals_(false),
      has_process_mmaps_(false),
      session_state_(session_state) {
}

ProcessMemoryDump::~ProcessMemoryDump() {
}

MemoryAllocatorDump* ProcessMemoryDump::CreateAllocatorDump(
    const std::string& absolute_name) {
  MemoryAllocatorDump* mad = new MemoryAllocatorDump(absolute_name, this);
  DCHECK_EQ(0ul, allocator_dumps_.count(absolute_name));
  allocator_dumps_storage_.push_back(mad);
  allocator_dumps_[absolute_name] = mad;
  return mad;
}

MemoryAllocatorDump* ProcessMemoryDump::GetAllocatorDump(
    const std::string& absolute_name) const {
  auto it = allocator_dumps_.find(absolute_name);
  return it == allocator_dumps_.end() ? nullptr : it->second;
}

void ProcessMemoryDump::TakeAllDumpsFrom(ProcessMemoryDump* other) {
  // We support only merging of MemoryAllocatorDumps. The special process_totals
  // and mmaps cases are not relevant, let's just prevent clients from doing it.
  DCHECK(!other->has_process_totals() && !other->has_process_mmaps());

  // Moves the ownership of all MemoryAllocatorDump(s) contained in |other|
  // into this ProcessMemoryDump.
  for (MemoryAllocatorDump* mad : other->allocator_dumps_storage_) {
    // Check that we don't merge duplicates.
    DCHECK_EQ(0ul, allocator_dumps_.count(mad->absolute_name()));
    allocator_dumps_storage_.push_back(mad);
    allocator_dumps_[mad->absolute_name()] = mad;
  }
  other->allocator_dumps_storage_.weak_clear();
  other->allocator_dumps_.clear();
}

void ProcessMemoryDump::AsValueInto(TracedValue* value) const {
  // Build up the [dumper name] -> [value] dictionary.
  if (has_process_totals_) {
    value->BeginDictionary("process_totals");
    process_totals_.AsValueInto(value);
    value->EndDictionary();
  }
  if (has_process_mmaps_) {
    value->BeginDictionary("process_mmaps");
    process_mmaps_.AsValueInto(value);
    value->EndDictionary();
  }
  if (allocator_dumps_storage_.size() > 0) {
    value->BeginDictionary("allocators");
    for (const MemoryAllocatorDump* allocator_dump : allocator_dumps_storage_)
      allocator_dump->AsValueInto(value);
    value->EndDictionary();
  }
}

}  // namespace trace_event
}  // namespace base
