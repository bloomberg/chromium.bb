// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_MACH_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_MACH_H_

#include "base/memory/discardable_memory.h"

#include "base/mac/scoped_mach_vm.h"
#include "base/memory/discardable_memory_manager.h"

namespace base {
namespace internal {

class DiscardableMemoryMach
    : public DiscardableMemory,
      public internal::DiscardableMemoryManagerAllocation {
 public:
  explicit DiscardableMemoryMach(size_t bytes);
  virtual ~DiscardableMemoryMach();

  static void PurgeForTesting();

  bool Initialize();

  // Overridden from DiscardableMemory:
  virtual DiscardableMemoryLockStatus Lock() override;
  virtual void Unlock() override;
  virtual void* Memory() const override;

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  virtual bool AllocateAndAcquireLock() override;
  virtual void ReleaseLock() override;
  virtual void Purge() override;

 private:
  mac::ScopedMachVM memory_;
  const size_t bytes_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryMach);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MACH_H_
