// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem_allocator.h"

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/discardable_shared_memory.h"

namespace base {
namespace {

class DiscardableMemoryShmemChunkImpl : public DiscardableMemoryShmemChunk {
 public:
  explicit DiscardableMemoryShmemChunkImpl(
      scoped_ptr<DiscardableSharedMemory> shared_memory)
      : shared_memory_(shared_memory.Pass()) {}

  // Overridden from DiscardableMemoryShmemChunk:
  bool Lock() override { return false; }
  void Unlock() override {
    shared_memory_->Unlock(0, 0);
    shared_memory_.reset();
  }
  void* Memory() const override { return shared_memory_->memory(); }

 private:
  scoped_ptr<DiscardableSharedMemory> shared_memory_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryShmemChunkImpl);
};

// Default allocator implementation that allocates in-process
// DiscardableSharedMemory instances.
class DiscardableMemoryShmemAllocatorImpl
    : public DiscardableMemoryShmemAllocator {
 public:
  // Overridden from DiscardableMemoryShmemAllocator:
  scoped_ptr<DiscardableMemoryShmemChunk>
  AllocateLockedDiscardableMemory(size_t size) override {
    scoped_ptr<DiscardableSharedMemory> memory(new DiscardableSharedMemory);
    if (!memory->CreateAndMap(size))
      return nullptr;

    return make_scoped_ptr(new DiscardableMemoryShmemChunkImpl(memory.Pass()));
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
