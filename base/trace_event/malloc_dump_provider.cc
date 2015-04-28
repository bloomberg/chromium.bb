// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <malloc.h>

#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

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
bool MallocDumpProvider::OnMemoryDump(ProcessMemoryDump* pmd) {
  struct mallinfo info = mallinfo();
  DCHECK_GE(info.arena + info.hblkhd, info.uordblks);

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump("malloc");
  if (!dump)
    return false;

  // When the system allocator is implemented by tcmalloc, the total physical
  // size is given by |arena| and |hblkhd| is 0. In case of Android's jemalloc
  // |arena| is 0 and the outer pages size is reported by |hblkhd|. In case of
  // dlmalloc the total is given by |arena| + |hblkhd|.
  // For more details see link: http://goo.gl/fMR8lF.
  dump->AddScalar(MemoryAllocatorDump::kNameOuterSize,
                  MemoryAllocatorDump::kUnitsBytes, info.arena + info.hblkhd);

  // Total allocated space is given by |uordblks|.
  dump->AddScalar(MemoryAllocatorDump::kNameInnerSize,
                  MemoryAllocatorDump::kUnitsBytes, info.uordblks);

  return true;
}

}  // namespace trace_event
}  // namespace base
