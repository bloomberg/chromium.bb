// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_dump.h"

#include "base/trace_event/process_memory_totals.h"
#include "base/trace_event/trace_event_argument.h"

namespace base {
namespace trace_event {

ProcessMemoryDump::ProcessMemoryDump()
    : has_process_totals_(false), has_process_mmaps_(false) {
}

ProcessMemoryDump::~ProcessMemoryDump() {
}

MemoryAllocatorDump* ProcessMemoryDump::CreateAllocatorDump(
    const std::string& name) {
  return CreateAllocatorDump(name, nullptr);
}

MemoryAllocatorDump* ProcessMemoryDump::CreateAllocatorDump(
    const std::string& name,
    MemoryAllocatorDump* parent) {
  DCHECK_EQ(0ul, allocator_dumps_.count(name));
  MemoryAllocatorDump* mad = new MemoryAllocatorDump(name, parent);
  allocator_dumps_storage_.push_back(mad);
  allocator_dumps_[name] = mad;
  return mad;
}

MemoryAllocatorDump* ProcessMemoryDump::GetAllocatorDump(
    const std::string& name) const {
  auto it = allocator_dumps_.find(name);
  return it == allocator_dumps_.end() ? nullptr : it->second;
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
