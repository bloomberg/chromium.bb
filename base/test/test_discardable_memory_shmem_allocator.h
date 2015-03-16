// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_
#define BASE_TEST_TEST_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_

#include "base/memory/discardable_memory_shmem_allocator.h"

namespace base {

class TestDiscardableMemoryShmemAllocator
    : public DiscardableMemoryShmemAllocator {
 public:
  TestDiscardableMemoryShmemAllocator();
  ~TestDiscardableMemoryShmemAllocator() override;

  // Overridden from DiscardableMemoryShmemAllocator:
  scoped_ptr<DiscardableMemoryShmemChunk> AllocateLockedDiscardableMemory(
      size_t size) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestDiscardableMemoryShmemAllocator);
};

}  // namespace base

#endif  // BASE_TEST_TEST_DISCARDABLE_MEMORY_SHMEM_ALLOCATOR_H_
