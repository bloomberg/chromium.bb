// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem_allocator.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_shared_memory.h"

namespace base {
namespace {

DiscardableMemoryShmemAllocator* g_allocator = nullptr;

}  // namespace

// static
void DiscardableMemoryShmemAllocator::SetInstance(
    DiscardableMemoryShmemAllocator* allocator) {
  DCHECK(allocator);

  // Make sure this function is only called once before the first call
  // to GetInstance().
  DCHECK(!g_allocator);

  g_allocator = allocator;
}

// static
DiscardableMemoryShmemAllocator*
DiscardableMemoryShmemAllocator::GetInstance() {
  DCHECK(g_allocator);
  return g_allocator;
}

}  // namespace base
