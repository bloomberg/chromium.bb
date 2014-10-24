// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_

#include "base/memory/discardable_memory.h"

#include "base/memory/discardable_memory_manager.h"

namespace base {
namespace internal {

class DiscardableMemoryEmulated
    : public DiscardableMemory,
      public internal::DiscardableMemoryManagerAllocation {
 public:
  explicit DiscardableMemoryEmulated(size_t bytes);
  ~DiscardableMemoryEmulated() override;

  static bool ReduceMemoryUsage();

  // TODO(reveman): Remove this as it is breaking the discardable memory design
  // principle that implementations should not rely on information this is
  // unavailable in kernel space. crbug.com/400423
  BASE_EXPORT static void ReduceMemoryUsageUntilWithinLimit(size_t bytes);

  static void PurgeForTesting();

  bool Initialize();

  // Overridden from DiscardableMemory:
  DiscardableMemoryLockStatus Lock() override;
  void Unlock() override;
  void* Memory() const override;

  // Overridden from internal::DiscardableMemoryManagerAllocation:
  bool AllocateAndAcquireLock() override;
  void ReleaseLock() override {}
  void Purge() override;
  bool IsMemoryResident() const override;

 private:
  const size_t bytes_;
  scoped_ptr<uint8[]> memory_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryEmulated);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_EMULATED_H_
