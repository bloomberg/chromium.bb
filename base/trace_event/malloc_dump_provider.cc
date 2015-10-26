// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <malloc.h>

#include "base/allocator/allocator_extension_thunks.h"
#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

// static
const char MallocDumpProvider::kAllocatedObjects[] = "malloc/allocated_objects";

// static
MallocDumpProvider* MallocDumpProvider::GetInstance() {
  return Singleton<MallocDumpProvider,
                   LeakySingletonTraits<MallocDumpProvider>>::get();
}

MallocDumpProvider::MallocDumpProvider() {
}

MallocDumpProvider::~MallocDumpProvider() {
}

// Called at trace dump point time. Creates a snapshot the memory counters for
// the current process.
bool MallocDumpProvider::OnMemoryDump(const MemoryDumpArgs& args,
                                      ProcessMemoryDump* pmd) {
  size_t total_virtual_size = 0;
  size_t resident_size = 0;
  size_t allocated_objects_size = 0;

  allocator::thunks::GetNumericPropertyFunction get_property_function =
      allocator::thunks::GetGetNumericPropertyFunction();
  if (get_property_function) {
    // If the function is not null then tcmalloc is used. See
    // MallocExtension::getNumericProperty.
    size_t pageheap_unmapped_bytes = 0;
    bool res = get_property_function("generic.heap_size", &total_virtual_size);
    DCHECK(res);
    res = get_property_function("tcmalloc.pageheap_unmapped_bytes",
                                &pageheap_unmapped_bytes);
    DCHECK(res);
    res = get_property_function("generic.current_allocated_bytes",
                                &allocated_objects_size);
    DCHECK(res);

    // Please see TCMallocImplementation::GetStats implementation for
    // explanation
    // about this math.
    // TODO(ssid): Usage of metadata is not included in page heap bytes
    // (crbug.com/546491). MallocExtension::GetNumericProperty will be extended
    // to get this value.
    resident_size = total_virtual_size - pageheap_unmapped_bytes;
  } else {
    struct mallinfo info = mallinfo();
    DCHECK_GE(info.arena + info.hblkhd, info.uordblks);

    // In case of Android's jemalloc |arena| is 0 and the outer pages size is
    // reported by |hblkhd|. In case of dlmalloc the total is given by
    // |arena| + |hblkhd|. For more details see link: http://goo.gl/fMR8lF.
    total_virtual_size = info.arena + info.hblkhd;
    resident_size = info.uordblks;
    allocated_objects_size = info.uordblks;
  }

  MemoryAllocatorDump* outer_dump = pmd->CreateAllocatorDump("malloc");
  outer_dump->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                        total_virtual_size);
  outer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, resident_size);

  // Total allocated space is given by |uordblks|.
  MemoryAllocatorDump* inner_dump = pmd->CreateAllocatorDump(kAllocatedObjects);
  inner_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes,
                        allocated_objects_size);

  return true;
}

}  // namespace trace_event
}  // namespace base
