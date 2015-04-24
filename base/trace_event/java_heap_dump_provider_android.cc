// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/java_heap_dump_provider_android.h"

#include "base/android/java_runtime.h"
#include "base/trace_event/process_memory_dump.h"

namespace base {
namespace trace_event {

namespace {

const char kDumperFriendlyName[] = "JavaHeap";
const char kDumperName[] = "java_heap";

}  // namespace

// static
JavaHeapDumpProvider* JavaHeapDumpProvider::GetInstance() {
  return Singleton<JavaHeapDumpProvider,
                   LeakySingletonTraits<JavaHeapDumpProvider>>::get();
}

JavaHeapDumpProvider::JavaHeapDumpProvider() {
}

JavaHeapDumpProvider::~JavaHeapDumpProvider() {
}

// Called at trace dump point time. Creates a snapshot with the memory counters
// for the current process.
bool JavaHeapDumpProvider::DumpInto(ProcessMemoryDump* pmd) {
  MemoryAllocatorDump* dump =
      pmd->CreateAllocatorDump(kDumperName, MemoryAllocatorDump::kRootHeap);

  // These numbers come from java.lang.Runtime stats.
  long total_heap_size = 0;
  long free_heap_size = 0;
  android::JavaRuntime::GetMemoryUsage(&total_heap_size, &free_heap_size);
  dump->set_physical_size_in_bytes(total_heap_size);
  dump->set_allocated_objects_count(0);
  dump->set_allocated_objects_size_in_bytes(total_heap_size - free_heap_size);
  return true;
}

const char* JavaHeapDumpProvider::GetFriendlyName() const {
  return kDumperFriendlyName;
}

}  // namespace trace_event
}  // namespace base
