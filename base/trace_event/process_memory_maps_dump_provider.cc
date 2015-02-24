// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/process_memory_maps_dump_provider.h"

#include <fstream>

#include "base/logging.h"
#include "base/process/process_metrics.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/process_memory_maps.h"

namespace base {
namespace trace_event {

#if defined(OS_LINUX) || defined(OS_ANDROID)
// static
std::istream* ProcessMemoryMapsDumpProvider::proc_smaps_for_testing = nullptr;

namespace {
uint32 ReadLinuxProcSmapsFile(std::istream* smaps, ProcessMemoryMaps* pmm) {
  if (!smaps->good()) {
    LOG(ERROR) << "Could not read smaps file.";
    return 0;
  }
  uint32 num_regions_processed = 0;
  // TODO(primiano): in next CLs add the actual code to process the smaps file.
  return num_regions_processed;
}
}  // namespace
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// static
ProcessMemoryMapsDumpProvider* ProcessMemoryMapsDumpProvider::GetInstance() {
  return Singleton<ProcessMemoryMapsDumpProvider,
                   LeakySingletonTraits<ProcessMemoryMapsDumpProvider>>::get();
}

ProcessMemoryMapsDumpProvider::ProcessMemoryMapsDumpProvider() {
}

ProcessMemoryMapsDumpProvider::~ProcessMemoryMapsDumpProvider() {
}

// Called at trace dump point time. Creates a snapshot the memory maps for the
// current process.
void ProcessMemoryMapsDumpProvider::DumpInto(ProcessMemoryDump* pmd) {
  uint32 res = 0;

#if defined(OS_LINUX) || defined(OS_ANDROID)
  if (UNLIKELY(proc_smaps_for_testing)) {
    res = ReadLinuxProcSmapsFile(proc_smaps_for_testing, pmd->process_mmaps());
  } else {
    std::ifstream proc_self_smaps("/proc/self/smaps");
    res = ReadLinuxProcSmapsFile(&proc_self_smaps, pmd->process_mmaps());
  }
#else
  LOG(ERROR) << "ProcessMemoryMaps dump provider is supported only on Linux";
#endif

  if (res > 0)
    pmd->set_has_process_mmaps();
}

}  // namespace trace_event
}  // namespace base
