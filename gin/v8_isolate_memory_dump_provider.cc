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

V8IsolateMemoryDumpProvider::V8IsolateMemoryDumpProvider(
    IsolateHolder* isolate_holder)
    : isolate_holder_(isolate_holder) {
  base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
      this, "V8Isolate", base::ThreadTaskRunnerHandle::Get());
}

V8IsolateMemoryDumpProvider::~V8IsolateMemoryDumpProvider() {
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current isolate.
bool V8IsolateMemoryDumpProvider::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  // TODO(ssid): Use MemoryDumpArgs to create light dumps when requested
  // (crbug.com/499731).

  if (isolate_holder_->access_mode() == IsolateHolder::kUseLocker) {
    v8::Locker locked(isolate_holder_->isolate());
    DumpHeapStatistics(args, process_memory_dump);
  } else {
    DumpHeapStatistics(args, process_memory_dump);
  }
  return true;
}

void V8IsolateMemoryDumpProvider::DumpHeapStatistics(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* process_memory_dump) {
  std::string dump_base_name =
      base::StringPrintf("v8/isolate_%p", isolate_holder_->isolate());

  // Dump statistics of the heap's spaces.
  std::string space_name_prefix = dump_base_name + "/heap_spaces";
  v8::HeapStatistics heap_statistics;
  isolate_holder_->isolate()->GetHeapStatistics(&heap_statistics);

  size_t known_spaces_used_size = 0;
  size_t known_spaces_size = 0;
  size_t known_spaces_physical_size = 0;
  size_t number_of_spaces = isolate_holder_->isolate()->NumberOfHeapSpaces();
  for (size_t space = 0; space < number_of_spaces; space++) {
    v8::HeapSpaceStatistics space_statistics;
    isolate_holder_->isolate()->GetHeapSpaceStatistics(&space_statistics,
                                                       space);
    const size_t space_size = space_statistics.space_size();
    const size_t space_used_size = space_statistics.space_used_size();
    const size_t space_physical_size = space_statistics.physical_space_size();

    known_spaces_size += space_size;
    known_spaces_used_size += space_used_size;
    known_spaces_physical_size += space_physical_size;

    std::string space_dump_name =
        space_name_prefix + "/" + space_statistics.space_name();
    auto space_dump = process_memory_dump->CreateAllocatorDump(space_dump_name);
    space_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_physical_size);

    space_dump->AddScalar("virtual_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_size);

    space_dump->AddScalar("allocated_objects_size",
                          base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                          space_used_size);
  }

  // Compute the rest of the memory, not accounted by the spaces above.
  std::string other_spaces_name = space_name_prefix + "/other_spaces";
  auto other_dump = process_memory_dump->CreateAllocatorDump(other_spaces_name);

  other_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.total_physical_size() - known_spaces_physical_size);

  other_dump->AddScalar(
      "allocated_objects_size",
      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
      heap_statistics.used_heap_size() - known_spaces_used_size);

  other_dump->AddScalar("virtual_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        heap_statistics.total_heap_size() - known_spaces_size);

  // If V8 zaps garbage, all the memory mapped regions become resident,
  // so we add an extra dump to avoid mismatches w.r.t. the total
  // resident values.
  if (heap_statistics.does_zap_garbage()) {
    auto zap_dump = process_memory_dump->CreateAllocatorDump(
        dump_base_name + "/zapped_for_debug");
    zap_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        heap_statistics.total_heap_size() -
                            heap_statistics.total_physical_size());
  }

  // If light dump is requested, then object statistics are not dumped
  if (args.level_of_detail == base::trace_event::MemoryDumpLevelOfDetail::LIGHT)
    return;

  // Dump statistics of the heap's live objects from last GC.
  // TODO(primiano): these should not be tracked in the same trace event as they
  // report stats for the last GC (not the current state). See crbug.com/498779.
  std::string object_name_prefix = dump_base_name + "/heap_objects_at_last_gc";
  bool did_dump_object_stats = false;
  const size_t object_types =
      isolate_holder_->isolate()->NumberOfTrackedHeapObjectTypes();
  for (size_t type_index = 0; type_index < object_types; type_index++) {
    v8::HeapObjectStatistics object_statistics;
    if (!isolate_holder_->isolate()->GetHeapObjectStatisticsAtLastGC(
            &object_statistics, type_index))
      continue;

    std::string dump_name =
        object_name_prefix + "/" + object_statistics.object_type();
    if (object_statistics.object_sub_type()[0] != '\0')
      dump_name += std::string("/") + object_statistics.object_sub_type();
    auto object_dump = process_memory_dump->CreateAllocatorDump(dump_name);

    object_dump->AddScalar(
        base::trace_event::MemoryAllocatorDump::kNameObjectCount,
        base::trace_event::MemoryAllocatorDump::kUnitsObjects,
        object_statistics.object_count());
    object_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                           base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                           object_statistics.object_size());
    did_dump_object_stats = true;
  }

  if (process_memory_dump->GetAllocatorDump(object_name_prefix +
                                            "/CODE_TYPE")) {
    auto code_kind_dump = process_memory_dump->CreateAllocatorDump(
        object_name_prefix + "/CODE_TYPE/CODE_KIND");
    auto code_age_dump = process_memory_dump->CreateAllocatorDump(
        object_name_prefix + "/CODE_TYPE/CODE_AGE");
    process_memory_dump->AddOwnershipEdge(code_kind_dump->guid(),
                                          code_age_dump->guid());
  }

  if (did_dump_object_stats) {
    process_memory_dump->AddOwnershipEdge(
        process_memory_dump->CreateAllocatorDump(object_name_prefix)->guid(),
        process_memory_dump->CreateAllocatorDump(space_name_prefix)->guid());
  }
}

}  // namespace gin
