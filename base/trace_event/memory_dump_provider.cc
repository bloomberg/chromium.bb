// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_provider.h"

#include "base/logging.h"

namespace base {
namespace trace_event {

MemoryDumpProvider::MemoryDumpProvider() {
}

MemoryDumpProvider::~MemoryDumpProvider() {
}

void MemoryDumpProvider::DeclareAllocatorAttribute(
    const MemoryAllocatorDeclaredAttribute& attr) {
  DCHECK_EQ(0u, allocator_attributes_.count(attr.name))
      << "Allocator attribute " << attr.name << " already declared for dumper "
      << GetFriendlyName();
  allocator_attributes_[attr.name] = attr;
}

}  // namespace trace_event
}  // namespace base
