// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem.h"

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory_shmem_allocator.h"

namespace base {
namespace internal {

DiscardableMemoryShmem::DiscardableMemoryShmem(size_t bytes)
    : bytes_(bytes), is_locked_(false) {
}

DiscardableMemoryShmem::~DiscardableMemoryShmem() {
  if (is_locked_)
    Unlock();
}

bool DiscardableMemoryShmem::Initialize() {
  return Lock() != DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;
}

DiscardableMemoryLockStatus DiscardableMemoryShmem::Lock() {
  DCHECK(!is_locked_);

  if (chunk_ && chunk_->Lock()) {
    is_locked_ = true;
    return DISCARDABLE_MEMORY_LOCK_STATUS_SUCCESS;
  }

  chunk_ = DiscardableMemoryShmemAllocator::GetInstance()
               ->AllocateLockedDiscardableMemory(bytes_);
  if (!chunk_)
    return DISCARDABLE_MEMORY_LOCK_STATUS_FAILED;

  is_locked_ = true;
  return DISCARDABLE_MEMORY_LOCK_STATUS_PURGED;
}

void DiscardableMemoryShmem::Unlock() {
  DCHECK(is_locked_);
  DCHECK(chunk_);
  chunk_->Unlock();
  is_locked_ = false;
}

void* DiscardableMemoryShmem::Memory() const {
  DCHECK(is_locked_);
  DCHECK(chunk_);
  return chunk_->Memory();
}

}  // namespace internal
}  // namespace base
