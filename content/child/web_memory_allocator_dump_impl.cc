// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_memory_allocator_dump_impl.h"

#include "base/trace_event/memory_allocator_dump.h"

namespace content {

WebMemoryAllocatorDumpImpl::WebMemoryAllocatorDumpImpl(
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump)
    : memory_allocator_dump_(memory_allocator_dump) {
}

WebMemoryAllocatorDumpImpl::~WebMemoryAllocatorDumpImpl() {
}

void WebMemoryAllocatorDumpImpl::AddScalar(const blink::WebString& name,
                                           const char* units,
                                           uint64 value) {
  memory_allocator_dump_->AddScalar(name.utf8(), units, value);
}

void WebMemoryAllocatorDumpImpl::AddString(const blink::WebString& name,
                                           const char* units,
                                           const blink::WebString& value) {
  memory_allocator_dump_->AddString(name.utf8(), units, value.utf8());
}
}  // namespace content
