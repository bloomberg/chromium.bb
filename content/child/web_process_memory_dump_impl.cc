// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_process_memory_dump_impl.h"

#include <stddef.h>

#include "base/memory/discardable_memory.h"
#include "base/trace_event/process_memory_dump.h"
#include "content/child/web_memory_allocator_dump_impl.h"
#include "skia/ext/skia_trace_memory_dump_impl.h"

namespace content {

WebProcessMemoryDumpImpl::WebProcessMemoryDumpImpl()
    : owned_process_memory_dump_(
          new base::trace_event::ProcessMemoryDump(nullptr)),
      process_memory_dump_(owned_process_memory_dump_.get()),
      level_of_detail_(base::trace_event::MemoryDumpLevelOfDetail::DETAILED) {}

WebProcessMemoryDumpImpl::WebProcessMemoryDumpImpl(
    base::trace_event::MemoryDumpLevelOfDetail level_of_detail,
    base::trace_event::ProcessMemoryDump* process_memory_dump)
    : process_memory_dump_(process_memory_dump),
      level_of_detail_(level_of_detail) {}

WebProcessMemoryDumpImpl::~WebProcessMemoryDumpImpl() {
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDumpImpl::createMemoryAllocatorDump(
    const blink::WebString& absolute_name) {
  // Get a MemoryAllocatorDump from the base/ object.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->CreateAllocatorDump(absolute_name.utf8());

  return createWebMemoryAllocatorDump(memory_allocator_dump);
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDumpImpl::createMemoryAllocatorDump(
    const blink::WebString& absolute_name,
    blink::WebMemoryAllocatorDumpGuid guid) {
  // Get a MemoryAllocatorDump from the base/ object with given guid.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->CreateAllocatorDump(
          absolute_name.utf8(),
          base::trace_event::MemoryAllocatorDumpGuid(guid));
  return createWebMemoryAllocatorDump(memory_allocator_dump);
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDumpImpl::createWebMemoryAllocatorDump(
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump) {
  if (!memory_allocator_dump)
    return nullptr;

  // Wrap it and return to blink.
  WebMemoryAllocatorDumpImpl* web_memory_allocator_dump_impl =
      new WebMemoryAllocatorDumpImpl(memory_allocator_dump);

  // memory_allocator_dumps_ will take ownership of
  // |web_memory_allocator_dumpd_impl|.
  memory_allocator_dumps_.set(memory_allocator_dump,
                              make_scoped_ptr(web_memory_allocator_dump_impl));
  return web_memory_allocator_dump_impl;
}

blink::WebMemoryAllocatorDump* WebProcessMemoryDumpImpl::getMemoryAllocatorDump(
    const blink::WebString& absolute_name) const {
  // Retrieve the base MemoryAllocatorDump object and then reverse lookup
  // its wrapper.
  base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
      process_memory_dump_->GetAllocatorDump(absolute_name.utf8());
  if (!memory_allocator_dump)
    return nullptr;

  // The only case of (memory_allocator_dump && !web_memory_allocator_dump)
  // is something from blink trying to get a MAD that was created from chromium,
  // which is an odd use case.
  blink::WebMemoryAllocatorDump* web_memory_allocator_dump =
      memory_allocator_dumps_.get(memory_allocator_dump);
  DCHECK(web_memory_allocator_dump);
  return web_memory_allocator_dump;
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
  // underlying ProcessMemoryDump, we need to:

  // 1) Move and transfer the ownership of the wrapped
  // base::trace_event::MemoryAllocatorDump(s) instances.
  process_memory_dump_->TakeAllDumpsFrom(other_impl->process_memory_dump_);

  // 2) Move and transfer the ownership of the WebMemoryAllocatorDump wrappers.
  const size_t expected_final_size = memory_allocator_dumps_.size() +
                                     other_impl->memory_allocator_dumps_.size();
  while (!other_impl->memory_allocator_dumps_.empty()) {
    auto first_entry = other_impl->memory_allocator_dumps_.begin();
    base::trace_event::MemoryAllocatorDump* memory_allocator_dump =
        first_entry->first;
    memory_allocator_dumps_.set(
        memory_allocator_dump,
        other_impl->memory_allocator_dumps_.take_and_erase(first_entry));
  }
  DCHECK_EQ(expected_final_size, memory_allocator_dumps_.size());
  DCHECK(other_impl->memory_allocator_dumps_.empty());
}

void WebProcessMemoryDumpImpl::addOwnershipEdge(
    blink::WebMemoryAllocatorDumpGuid source,
    blink::WebMemoryAllocatorDumpGuid target,
    int importance) {
  process_memory_dump_->AddOwnershipEdge(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      base::trace_event::MemoryAllocatorDumpGuid(target), importance);
}

void WebProcessMemoryDumpImpl::addOwnershipEdge(
    blink::WebMemoryAllocatorDumpGuid source,
    blink::WebMemoryAllocatorDumpGuid target) {
  process_memory_dump_->AddOwnershipEdge(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      base::trace_event::MemoryAllocatorDumpGuid(target));
}

void WebProcessMemoryDumpImpl::addSuballocation(
    blink::WebMemoryAllocatorDumpGuid source,
    const blink::WebString& target_node_name) {
  process_memory_dump_->AddSuballocation(
      base::trace_event::MemoryAllocatorDumpGuid(source),
      target_node_name.utf8());
}

SkTraceMemoryDump* WebProcessMemoryDumpImpl::createDumpAdapterForSkia(
    const blink::WebString& dump_name_prefix) {
  sk_trace_dump_list_.push_back(new skia::SkiaTraceMemoryDumpImpl(
      dump_name_prefix.utf8(), level_of_detail_, process_memory_dump_));
  return sk_trace_dump_list_.back();
}

blink::WebMemoryAllocatorDump*
WebProcessMemoryDumpImpl::CreateDiscardableMemoryAllocatorDump(
    const std::string& name,
    base::DiscardableMemory* discardable) {
  base::trace_event::MemoryAllocatorDump* dump =
      discardable->CreateMemoryAllocatorDump(name.c_str(),
                                             process_memory_dump_);
  return createWebMemoryAllocatorDump(dump);
}

}  // namespace content
