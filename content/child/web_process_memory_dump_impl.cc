// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_process_memory_dump_impl.h"

#include "base/trace_event/process_memory_dump.h"
#include "content/child/web_memory_allocator_dump_impl.h"

namespace content {

WebProcessMemoryDumpImpl::WebProcessMemoryDumpImpl(
    base::trace_event::ProcessMemoryDump* process_memory_dump)
    : process_memory_dump_(process_memory_dump) {
}

WebProcessMemoryDumpImpl::~WebProcessMemoryDumpImpl() {
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDumpImpl::createMemoryAllocatorDump(
    const blink::WebString& absolute_name) {
  // Get a MemoryAllocatorDump from the base/ object.
  base::trace_event::MemoryAllocatorDump* mad =
      process_memory_dump_->CreateAllocatorDump(absolute_name.utf8());
  if (!mad)
    return nullptr;

  // Wrap it and return to blink.
  WebMemoryAllocatorDumpImpl* web_mad_impl =
      new WebMemoryAllocatorDumpImpl(mad);

  // memory_allocator_dumps_ will take ownership of |web_mad_impl|.
  memory_allocator_dumps_.push_back(web_mad_impl);
  return web_mad_impl;
}

}  // namespace content
