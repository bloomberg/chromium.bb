// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_isolate_memory_dump_provider.h"

#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "gin/public/isolate_holder.h"
#include "v8/include/v8.h"

namespace gin {

namespace {
const char kRootDumpName[] = "v8";
const char kIsolateDumpName[] = "isolate";
const char kHeapSpacesDumpName[] = "heap_spaces";
const char kAvailableSizeAttribute[] = "available_size_in_bytes";
}  // namespace

V8IsolateMemoryDumpProvider::V8IsolateMemoryDumpProvider(
    IsolateHolder* isolate_holder)
    : isolate_holder_(isolate_holder) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, base::ThreadTaskRunnerHandle::Get());
}

V8IsolateMemoryDumpProvider::~V8IsolateMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current isolate.
bool V8IsolateMemoryDumpProvider::OnMemoryDump(
    base::trace_event::ProcessMemoryDump* pmd) {
  if (isolate_holder_->access_mode() == IsolateHolder::kUseLocker) {
    v8::Locker locked(isolate_holder_->isolate());
    DumpMemoryStatistics(pmd);
  } else {
    DumpMemoryStatistics(pmd);
  }
  return true;
}

void V8IsolateMemoryDumpProvider::DumpMemoryStatistics(
    base::trace_event::ProcessMemoryDump* pmd) {
  v8::HeapStatistics heap_statistics;
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);

  size_t known_spaces_used_size = 0;
  size_t known_spaces_size = 0;
  size_t number_of_spaces = isolate_holder_->isolate()->NumberOfHeapSpaces();
  for (size_t space = 0; space < number_of_spaces; space++) {
    v8::HeapSpaceStatistics space_statistics;
    isolate_holder_->isolate()->GetHeapSpaceStatistics(&space_statistics,
                                                       space);
    const size_t space_size = space_statistics.space_size();
    const size_t space_used_size = space_statistics.space_used_size();

    known_spaces_size += space_size;
    known_spaces_used_size += space_used_size;

    std::string allocator_name =
        base::StringPrintf("%s/%s_%p/%s/%s", kRootDumpName, kIsolateDumpName,
                           isolate_holder_->isolate(), kHeapSpacesDumpName,
                           space_statistics.space_name());
    base::trace_event::MemoryAllocatorDump* space_dump =
        pmd->CreateAllocatorDump(allocator_name);
    space_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameOuterSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, space_size);

    // TODO(ssid): Fix crbug.com/481504 to get the objects count of live objects
    // after the last GC.
    space_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameInnerSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, space_used_size);
    space_dump->AddScalar(kAvailableSizeAttribute,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_statistics.space_available_size());
  }
  // Compute the rest of the memory, not accounted by the spaces above.
  std::string allocator_name = base::StringPrintf(
      "%s/%s_%p/%s/%s", kRootDumpName, kIsolateDumpName,
      isolate_holder_->isolate(), kHeapSpacesDumpName, "other_spaces");
  base::trace_event::MemoryAllocatorDump* other_spaces_dump =
      pmd->CreateAllocatorDump(allocator_name);
  other_spaces_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameOuterSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.total_heap_size() - known_spaces_size);
  other_spaces_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameInnerSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.used_heap_size() - known_spaces_used_size);

  // TODO(ssid): Fix crbug.com/481504 to get the total available size of the
  // heap.
  other_spaces_dump->AddScalar(
      kAvailableSizeAttribute,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes, 0);
}

}  // namespace gin
