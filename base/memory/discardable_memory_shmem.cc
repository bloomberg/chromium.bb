// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_shmem.h"

#include "base/lazy_instance.h"
#include "base/memory/discardable_memory_shmem_allocator.h"

namespace base {
namespace internal {

DiscardableMemoryShmem::DiscardableMemoryShmem(size_t bytes)
    : chunk_(DiscardableMemoryShmemAllocator::GetInstance()
                 ->AllocateLockedDiscardableMemory(bytes)),
      is_locked_(true) {
  DCHECK(chunk_);
}

DiscardableMemoryShmem::~DiscardableMemoryShmem() {
  if (is_locked_)
    Unlock();
}

bool DiscardableMemoryShmem::Lock() {
  DCHECK(!is_locked_);
  DCHECK(chunk_);

  if (!chunk_->Lock()) {
    chunk_.reset();
    return false;
  }

  is_locked_ = true;
  return true;
}

void DiscardableMemoryShmem::Unlock() {
  DCHECK(is_locked_);
  chunk_->Unlock();
  is_locked_ = false;
}

void* DiscardableMemoryShmem::Memory() const {
  DCHECK(is_locked_);
  return chunk_->Memory();
}

}  // namespace internal
}  // namespace base
