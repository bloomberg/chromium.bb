// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_provider.h"

#include "base/single_thread_task_runner.h"

namespace base {
namespace trace_event {

MemoryDumpProvider::MemoryDumpProvider() {
}

MemoryDumpProvider::MemoryDumpProvider(
    const scoped_refptr<SingleThreadTaskRunner>& task_runner)
    : task_runner_(task_runner) {
}

MemoryDumpProvider::~MemoryDumpProvider() {
}

void MemoryDumpProvider::DeclareAllocatorAttribute(
    const std::string& allocator_name,
    const std::string& attribute_name,
    const std::string& attribute_type) {
  allocator_attributes_type_info_.Set(
      allocator_name, attribute_name, attribute_type);
}

}  // namespace trace_event
}  // namespace base
