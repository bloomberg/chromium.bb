// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_memory_allocator_dump_impl.h"

#include "base/trace_event/memory_allocator_dump.h"

namespace content {

WebMemoryAllocatorDumpImpl::WebMemoryAllocatorDumpImpl(
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump)
    : memory_allocator_dump_(memory_allocator_dump),
      guid_(memory_allocator_dump->guid().ToUint64()) {
}

WebMemoryAllocatorDumpImpl::~WebMemoryAllocatorDumpImpl() {
}

void WebMemoryAllocatorDumpImpl::addScalar(const char* name,
                                           const char* units,
                                           uint64_t value) {
  memory_allocator_dump_->AddScalar(name, units, value);
}

void WebMemoryAllocatorDumpImpl::addScalarF(const char* name,
                                            const char* units,
                                            double value) {
  memory_allocator_dump_->AddScalarF(name, units, value);
}

void WebMemoryAllocatorDumpImpl::addString(const char* name,
                                           const char* units,
                                           const blink::WebString& value) {
  memory_allocator_dump_->AddString(name, units, value.utf8());
}

blink::WebMemoryAllocatorDumpGuid WebMemoryAllocatorDumpImpl::guid() const {
  return guid_;
}
}  // namespace content
