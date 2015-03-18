// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/web_discardable_memory_impl.h"

#include "base/memory/discardable_memory.h"
#include "base/memory/discardable_memory_allocator.h"

namespace content {

WebDiscardableMemoryImpl::~WebDiscardableMemoryImpl() {}

// static
scoped_ptr<WebDiscardableMemoryImpl>
WebDiscardableMemoryImpl::CreateLockedMemory(size_t size) {
  return make_scoped_ptr(new WebDiscardableMemoryImpl(
      base::DiscardableMemoryAllocator::GetInstance()
          ->AllocateLockedDiscardableMemory(size)));
}

bool WebDiscardableMemoryImpl::lock() {
  return discardable_->Lock();
}

void WebDiscardableMemoryImpl::unlock() {
  discardable_->Unlock();
}

void* WebDiscardableMemoryImpl::data() {
  return discardable_->Memory();
}

WebDiscardableMemoryImpl::WebDiscardableMemoryImpl(
    scoped_ptr<base::DiscardableMemory> memory)
    : discardable_(memory.Pass()) {
}

}  // namespace content
