// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_

#include "base/base_export.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DiscardableSharedMemory;

class BASE_EXPORT DiscardableMemoryShmemAllocator {
 public:
  // Returns the allocator instance.
  static DiscardableMemoryShmemAllocator* GetInstance();

  // Sets the allocator instance. Can only be called once, e.g. on startup.
  // Ownership of |instance| remains with the caller.
  static void SetInstance(DiscardableMemoryShmemAllocator* allocator);

  virtual scoped_ptr<DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size) = 0;

 protected:
  virtual ~DiscardableMemoryShmemAllocator() {}
};

}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_
