// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_process_memory_dump_impl.h"

#include "base/trace_event/process_memory_dump.h"
#include "content/child/web_memory_allocator_dump_impl.h"

namespace content {

WebProcessMemoryDumpImpl::WebProcessMemoryDumpImpl()
    : owned_process_memory_dump_(
          new base::trace_event::ProcessMemoryDump(nullptr)),
      process_memory_dump_(owned_process_memory_dump_.get()) {
}

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

void WebProcessMemoryDumpImpl::clear() {
  // Clear all the WebMemoryAllocatorDump wrappers.
  memory_allocator_dumps_.clear();

  // Clear the actual MemoryAllocatorDump objects from the underlying PMD.
  process_memory_dump_->Clear();
}

void WebProcessMemoryDumpImpl::takeAllDumpsFrom(
    blink::WebProcessMemoryDump* other) {
  auto other_impl = static_cast<WebProcessMemoryDumpImpl*>(other);
  // WebProcessMemoryDumpImpl is a container of WebMemoryAllocatorDump(s) which
  // in turn are wrappers of base::trace_event::MemoryAllocatorDump(s).
  // In order to expose the move and ownership transfer semantics of the
  // underlying ProcessMemoryDump, we need to

  // 1) Move and transfer the ownership of the wrapped
  // base::trace_event::MemoryAllocatorDump(s) instances.
  process_memory_dump_->TakeAllDumpsFrom(other_impl->process_memory_dump_);

  // 2) Move and transfer the ownership of the WebMemoryAllocatorDump wrappers.
  memory_allocator_dumps_.insert(memory_allocator_dumps_.end(),
                                 other_impl->memory_allocator_dumps_.begin(),
                                 other_impl->memory_allocator_dumps_.end());
  other_impl->memory_allocator_dumps_.weak_clear();
}

}  // namespace content
