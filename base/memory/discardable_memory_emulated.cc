// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory.h"

#include "base/memory/discardable_memory_provider.h"

using base::internal::DiscardableMemoryProvider;

namespace base {
namespace {

class DiscardableMemoryEmulated : public DiscardableMemory {
 public:
  explicit DiscardableMemoryEmulated(size_t size) : is_locked_(false) {
    DiscardableMemoryProvider::GetInstance()->Register(this, size);
  }

  virtual ~DiscardableMemoryEmulated() {
    if (is_locked_)
      Unlock();
    DiscardableMemoryProvider::GetInstance()->Unregister(this);
  }

  // DiscardableMemory:
  virtual LockDiscardableMemoryStatus Lock() OVERRIDE {
    DCHECK(!is_locked_);

    bool purged = false;
    memory_ = DiscardableMemoryProvider::GetInstance()->Acquire(this, &purged);
    if (!memory_)
      return DISCARDABLE_MEMORY_FAILED;

    is_locked_ = true;
    return purged ? DISCARDABLE_MEMORY_PURGED : DISCARDABLE_MEMORY_SUCCESS;
  }

  virtual void Unlock() OVERRIDE {
    DCHECK(is_locked_);
    DiscardableMemoryProvider::GetInstance()->Release(this, memory_.Pass());
    is_locked_ = false;
  }

  virtual void* Memory() const OVERRIDE {
    DCHECK(memory_);
    return memory_.get();
  }

 private:
  scoped_ptr<uint8, FreeDeleter> memory_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryEmulated);
};

}  // namespace

// static
bool DiscardableMemory::SupportedNatively() {
  return false;
}

// static
scoped_ptr<DiscardableMemory> DiscardableMemory::CreateLockedMemory(
    size_t size) {
  scoped_ptr<DiscardableMemory> memory(new DiscardableMemoryEmulated(size));
  if (!memory)
    return scoped_ptr<DiscardableMemory>();
  if (memory->Lock() != DISCARDABLE_MEMORY_PURGED)
    return scoped_ptr<DiscardableMemory>();
  return memory.Pass();
}

// static
bool DiscardableMemory::PurgeForTestingSupported() {
  return true;
}

// static
void DiscardableMemory::PurgeForTesting() {
  DiscardableMemoryProvider::GetInstance()->PurgeAll();
}

}  // namespace base
