// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem_allocator.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_shared_memory.h"

namespace base {
namespace {

// Default allocator implementation that allocates in-process
// DiscardableSharedMemory instances.
class DiscardableMemoryShmemAllocatorImpl
    : public DiscardableMemoryShmemAllocator {
 public:
  // Overridden from DiscardableMemoryShmemAllocator:
  virtual scoped_ptr<DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size) override {
    scoped_ptr<DiscardableSharedMemory> memory(new DiscardableSharedMemory);
    if (!memory->CreateAndMap(size))
      return scoped_ptr<DiscardableSharedMemory>();

    return memory.Pass();
  }
};

LazyInstance<DiscardableMemoryShmemAllocatorImpl>::Leaky g_default_allocator =
    LAZY_INSTANCE_INITIALIZER;

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
  if (!g_allocator)
    g_allocator = g_default_allocator.Pointer();

  return g_allocator;
}

}  // namespace base
