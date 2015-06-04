// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/v8_isolate_memory_dump_provider.h"

#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/process_memory_dump.h"
#include "gin/public/isolate_holder.h"
#include "v8/include/v8.h"

namespace gin {

namespace {
const char kHeapSpacesDumpName[] = "heap_spaces";
const char kHeapObjectsDumpName[] = "heap_objects";
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
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  std::string dump_base_name =
      base::StringPrintf("v8/isolate_%p", isolate_holder_->isolate());
  std::string space_dump_name = dump_base_name + "/" + kHeapSpacesDumpName;
  std::string object_dump_name = dump_base_name + "/" + kHeapObjectsDumpName;
  process_memory_dump->AddOwnershipEdge(
      process_memory_dump->CreateAllocatorDump(object_dump_name)->guid(),
      process_memory_dump->CreateAllocatorDump(space_dump_name)->guid());

  if (isolate_holder_->access_mode() == IsolateHolder::kUseLocker) {
    v8::Locker locked(isolate_holder_->isolate());
    DumpHeapSpacesStatistics(process_memory_dump, space_dump_name);
    DumpHeapObjectStatistics(process_memory_dump, object_dump_name);
  } else {
    DumpHeapSpacesStatistics(process_memory_dump, space_dump_name);
    DumpHeapObjectStatistics(process_memory_dump, object_dump_name);
  }
  return true;
}

void V8IsolateMemoryDumpProvider::DumpHeapSpacesStatistics(
    base::trace_event::ProcessMemoryDump* process_memory_dump,
    const std::string& dump_base_name) {
  v8::HeapStatistics heap_statistics;
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);

  size_t known_spaces_used_size = 0;
  size_t known_spaces_size = 0;
  size_t known_spaces_available_size = 0;
  size_t number_of_spaces = isolate_holder_->isolate()->NumberOfHeapSpaces();
  for (size_t space = 0; space < number_of_spaces; space++) {
    v8::HeapSpaceStatistics space_statistics;
    isolate_holder_->isolate()->GetHeapSpaceStatistics(&space_statistics,
                                                       space);
    const size_t space_size = space_statistics.space_size();
    const size_t space_used_size = space_statistics.space_used_size();
    const size_t space_available_size = space_statistics.space_available_size();

    known_spaces_size += space_size;
    known_spaces_used_size += space_used_size;
    known_spaces_available_size += space_available_size;

    base::trace_event::MemoryAllocatorDump* space_dump =
        process_memory_dump->CreateAllocatorDump(dump_base_name + "/" +
                                                 space_statistics.space_name());
    space_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameOuterSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, space_size);
    space_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameInnerSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes, space_used_size);
    space_dump->AddScalar(kAvailableSizeAttribute,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_available_size);
  }

  // Compute the rest of the memory, not accounted by the spaces above.
  base::trace_event::MemoryAllocatorDump* other_spaces_dump =
      process_memory_dump->CreateAllocatorDump(dump_base_name +
                                               "/other_spaces");
  other_spaces_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameOuterSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.total_heap_size() - known_spaces_size);
  other_spaces_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameInnerSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.used_heap_size() - known_spaces_used_size);

  other_spaces_dump->AddScalar(
      kAvailableSizeAttribute,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.total_available_size() - known_spaces_available_size);
}

void V8IsolateMemoryDumpProvider::DumpHeapObjectStatistics(
    base::trace_event::ProcessMemoryDump* process_memory_dump,
    const std::string& dump_base_name) {
  const size_t object_types =
      isolate_holder_->isolate()->NumberOfTrackedHeapObjectTypes();
  for (size_t type_index = 0; type_index < object_types; type_index++) {
    v8::HeapObjectStatistics object_statistics;
    if (!isolate_holder_->isolate()->GetHeapObjectStatisticsAtLastGC(
            &object_statistics, type_index))
      continue;

    std::string dump_name =
        dump_base_name + "/" + object_statistics.object_type();
    if (object_statistics.object_sub_type()[0] != '\0')
      dump_name += std::string("/") + object_statistics.object_sub_type();
    base::trace_event::MemoryAllocatorDump* object_dump =
        process_memory_dump->CreateAllocatorDump(dump_name);

    object_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameObjectsCount,
        base::trace_event::MemoryAllocatorDump::kUnitsObjects,
        object_statistics.object_count());
    object_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameInnerSize,
        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
        object_statistics.object_size());
  }

  process_memory_dump->AddOwnershipEdge(
      process_memory_dump->CreateAllocatorDump(dump_base_name +
                                               "/CODE_TYPE/CODE_KIND")->guid(),
      process_memory_dump->CreateAllocatorDump(dump_base_name +
                                               "/CODE_TYPE/CODE_AGE")->guid());
}

}  // namespace gin
