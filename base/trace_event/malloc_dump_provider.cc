// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <stddef.h>

#include "base/allocator/allocator_extension.h"
#include "base/trace_event/process_memory_dump.h"
#include "build/build_config.h"

#if defined(OS_MACOSX)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif

namespace base {
namespace trace_event {

// static
const char MallocDumpProvider::kAllocatedObjects[] = "malloc/allocated_objects";

// static
MallocDumpProvider* MallocDumpProvider::GetInstance() {
  return Singleton<MallocDumpProvider,
                   LeakySingletonTraits<MallocDumpProvider>>::get();
}

MallocDumpProvider::MallocDumpProvider() {}

MallocDumpProvider::~MallocDumpProvider() {}

// Called at trace dump point time. Creates a snapshot the memory counters for
// the current process.
bool MallocDumpProvider::OnMemoryDump(const MemoryDumpArgs& args,
                                      ProcessMemoryDump* pmd) {
  size_t total_virtual_size = 0;
  size_t resident_size = 0;
  size_t allocated_objects_size = 0;
#if defined(USE_TCMALLOC)
  bool res =
      allocator::GetNumericProperty("generic.heap_size", &total_virtual_size);
  DCHECK(res);
  res = allocator::GetNumericProperty("generic.total_physical_bytes",
                                      &resident_size);
  DCHECK(res);
  res = allocator::GetNumericProperty("generic.current_allocated_bytes",
                                      &allocated_objects_size);
  DCHECK(res);
#elif defined(OS_MACOSX) || defined(OS_IOS)
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);
  total_virtual_size = stats.size_allocated;
  allocated_objects_size = stats.size_in_use;

  // The resident size is approximated to the max size in use, which would count
  // the total size of all regions other than the free bytes at the end of each
  // region. In each allocation region the allocations are rounded off to a
  // fixed quantum, so the excess region will not be resident.
  // See crrev.com/1531463004 for detailed explanation.
  resident_size = stats.max_size_in_use;
#else
  struct mallinfo info = mallinfo();
  DCHECK_GE(info.arena + info.hblkhd, info.uordblks);

  // In case of Android's jemalloc |arena| is 0 and the outer pages size is
  // reported by |hblkhd|. In case of dlmalloc the total is given by
  // |arena| + |hblkhd|. For more details see link: http://goo.gl/fMR8lF.
  total_virtual_size = info.arena + info.hblkhd;
  resident_size = info.uordblks;
  allocated_objects_size = info.uordblks;
#endif

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

  if (resident_size - allocated_objects_size > 0) {
    // Explicitly specify why is extra memory resident. In tcmalloc it accounts
    // for free lists and caches. In mac and ios it accounts for the
    // fragmentation and metadata.
    MemoryAllocatorDump* other_dump =
        pmd->CreateAllocatorDump("malloc/metadata_fragmentation_caches");
    other_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          resident_size - allocated_objects_size);
  }

  return true;
}

}  // namespace trace_event
}  // namespace base
