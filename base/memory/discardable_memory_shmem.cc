// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem.h"

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory_shmem_allocator.h"
#include "base/memory/discardable_shared_memory.h"

namespace base {
namespace {

// Have the DiscardableMemoryManager trigger in-process eviction
// when address space usage gets too high (e.g. 512 MBytes).
const size_t kMemoryLimit = 512 * 1024 * 1024;

// internal::DiscardableMemoryManager has an explicit constructor that takes
// a number of memory limit parameters. The LeakyLazyInstanceTraits doesn't
// handle the case. Thus, we need our own class here.
struct DiscardableMemoryManagerLazyInstanceTraits {
  // Leaky as discardable memory clients can use this after the exit handler
  // has been called.
  static const bool kRegisterOnExit = false;
#ifndef NDEBUG
  static const bool kAllowedToAccessOnNonjoinableThread = true;
#endif

  static internal::DiscardableMemoryManager* New(void* instance) {
    return new (instance) internal::DiscardableMemoryManager(
        kMemoryLimit, kMemoryLimit, TimeDelta::Max());
  }
  static void Delete(internal::DiscardableMemoryManager* instance) {
    instance->~DiscardableMemoryManager();
  }
};

LazyInstance<internal::DiscardableMemoryManager,
             DiscardableMemoryManagerLazyInstanceTraits> g_manager =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace internal {

DiscardableMemoryShmem::DiscardableMemoryShmem(size_t bytes)
    : bytes_(bytes), is_locked_(false) {
  g_manager.Pointer()->Register(this, bytes);
}

DiscardableMemoryShmem::~DiscardableMemoryShmem() {
  if (is_locked_)
    Unlock();
  g_manager.Pointer()->Unregister(this);
}

// static
void DiscardableMemoryShmem::ReleaseFreeMemory() {
  g_manager.Pointer()->ReleaseFreeMemory();
}

// static
void DiscardableMemoryShmem::PurgeForTesting() {
  g_manager.Pointer()->PurgeAll();
}

bool DiscardableMemoryShmem::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryShmem::Lock() {
  DCHECK(!is_locked_);

  bool purged = false;
  if (!g_manager.Pointer()->AcquireLock(this, &purged))
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  is_locked_ = true;
  return purged ? DISCARDABLE_MEMORY_LOCK_STATUS_PURGED
                : DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
}

void DiscardableMemoryShmem::Unlock() {
  DCHECK(is_locked_);
  g_manager.Pointer()->ReleaseLock(this);
  is_locked_ = false;
}

void* DiscardableMemoryShmem::Memory() const {
  DCHECK(is_locked_);
  DCHECK(shared_memory_);
  return shared_memory_->memory();
}

bool DiscardableMemoryShmem::AllocateAndAcquireLock() {
  if (shared_memory_ && shared_memory_->Lock())
    return true;

  // TODO(reveman): Allocate fixed size memory segments and use a free list to
  // improve performance and limit the number of file descriptors used.
  shared_memory_ = DiscardableMemoryShmemAllocator::GetInstance()
                       ->AllocateLockedDiscardableSharedMemory(bytes_);
  DCHECK(shared_memory_);
  return false;
}

void DiscardableMemoryShmem::ReleaseLock() {
  shared_memory_->Unlock();
}

void DiscardableMemoryShmem::Purge() {
  shared_memory_->Purge(Time());
  shared_memory_.reset();
}

bool DiscardableMemoryShmem::IsMemoryResident() const {
  return shared_memory_->IsMemoryResident();
}

}  // namespace internal
}  // namespace base
