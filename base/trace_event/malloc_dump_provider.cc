// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <malloc.h>

#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

namespace {

const char kDumperFriendlyName[] = "Malloc";
const char kDumperName[] = "malloc";

}  // namespace

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
bool MallocDumpProvider::DumpInto(ProcessMemoryDump* pmd) {
  struct mallinfo info = mallinfo();
  DCHECK(info.uordblks > 0);
  DCHECK_GE(info.arena + info.hblkhd, info.uordblks);

  MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(kDumperName);
  if (!dump)
    return false;

  // When the system allocator is implemented by tcmalloc, the total physical
  // size is given by |arena| and |hblkhd| is 0. In case of Android's jemalloc
  // |arena| is 0 and the outer pages size is reported by |hblkhd|. In case of
  // dlmalloc the total is given by |arena| + |hblkhd|.
  // For more details see link: http://goo.gl/fMR8lF.
  dump->set_physical_size_in_bytes(info.arena + info.hblkhd);

  // mallinfo doesn't support any allocated object count.
  dump->set_allocated_objects_count(0);

  // Total allocated space is given by |uordblks|.
  dump->set_allocated_objects_size_in_bytes(info.uordblks);

  return true;
}

const char* MallocDumpProvider::GetFriendlyName() const {
  return kDumperFriendlyName;
}

}  // namespace trace_event
}  // namespace base
