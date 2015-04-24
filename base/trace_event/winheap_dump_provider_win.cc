// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/winheap_dump_provider_win.h"

#include <windows.h>

#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

namespace {

const char kDumperFriendlyName[] = "winheap";

// Report a heap dump to a process memory dump. The |heap_info| structure
// contains the information about this heap, and |heap_name| will be used to
// represent it in the report.
bool ReportHeapDump(ProcessMemoryDump* pmd,
                    const WinHeapInfo& heap_info,
                    const std::string& heap_name) {
  MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(kDumperFriendlyName, heap_name);
  if (!dump)
    return false;
  dump->set_physical_size_in_bytes(heap_info.committed_size);
  dump->set_allocated_objects_count(heap_info.block_count);
  dump->set_allocated_objects_size_in_bytes(heap_info.allocated_size);
  return true;
}

}  // namespace

WinHeapDumpProvider* WinHeapDumpProvider::GetInstance() {
  return Singleton<WinHeapDumpProvider,
                   LeakySingletonTraits<WinHeapDumpProvider>>::get();
}

bool WinHeapDumpProvider::DumpInto(ProcessMemoryDump* pmd) {
  // Retrieves the number of heaps in the current process.
  DWORD number_of_heaps = ::GetProcessHeaps(0, NULL);
  WinHeapInfo all_heap_info = {0};

  // Try to retrieve a handle to all the heaps owned by this process. Returns
  // false if the number of heaps has changed.
  //
  // This is inherently racy as is, but it's not something that we observe a lot
  // in Chrome, the heaps tend to be created at startup only.
  scoped_ptr<HANDLE[]> all_heaps(new HANDLE[number_of_heaps]);
  if (::GetProcessHeaps(number_of_heaps, all_heaps.get()) != number_of_heaps)
    return false;

  // Skip the pointer to the heap array to avoid accounting the memory used by
  // this dump provider.
  std::set<void*> block_to_skip;
  block_to_skip.insert(all_heaps.get());

  // Retrieves some metrics about each heap.
  for (size_t i = 0; i < number_of_heaps; ++i) {
    WinHeapInfo heap_info = {0};
    heap_info.heap_id = all_heaps[i];
    GetHeapInformation(&heap_info, block_to_skip);

    all_heap_info.allocated_size += heap_info.allocated_size;
    all_heap_info.committed_size += heap_info.committed_size;
    all_heap_info.block_count += heap_info.block_count;
  }
  // Report the heap dump.
  if (!ReportHeapDump(pmd, all_heap_info, MemoryAllocatorDump::kRootHeap))
    return false;

  return true;
}

const char* WinHeapDumpProvider::GetFriendlyName() const {
  return kDumperFriendlyName;
}

bool WinHeapDumpProvider::GetHeapInformation(
    WinHeapInfo* heap_info,
    const std::set<void*>& block_to_skip) {
  CHECK(::HeapLock(heap_info->heap_id) == TRUE);
  PROCESS_HEAP_ENTRY heap_entry;
  heap_entry.lpData = nullptr;
  // Walk over all the entries in this heap.
  while (::HeapWalk(heap_info->heap_id, &heap_entry) != FALSE) {
    if (block_to_skip.count(heap_entry.lpData) == 1)
      continue;
    if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
      heap_info->allocated_size += heap_entry.cbData;
      heap_info->block_count++;
    } else if ((heap_entry.wFlags & PROCESS_HEAP_REGION) != 0) {
      heap_info->committed_size += heap_entry.Region.dwCommittedSize;
    }
  }
  CHECK(::HeapUnlock(heap_info->heap_id) == TRUE);
  return true;
}

}  // namespace trace_event
}  // namespace base
