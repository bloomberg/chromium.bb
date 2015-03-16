// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_discardable_memory_shmem_allocator.h"

#include <stdint.h>

namespace base {
namespace {

class DiscardableMemoryShmemChunkImpl : public DiscardableMemoryShmemChunk {
 public:
  explicit DiscardableMemoryShmemChunkImpl(size_t size)
      : memory_(new uint8_t[size]) {}

  // Overridden from DiscardableMemoryShmemChunk:
  bool Lock() override { return false; }
  void Unlock() override {}
  void* Memory() const override { return memory_.get(); }

 private:
  scoped_ptr<uint8_t[]> memory_;
};

}  // namespace

TestDiscardableMemoryShmemAllocator::TestDiscardableMemoryShmemAllocator() {
}

TestDiscardableMemoryShmemAllocator::~TestDiscardableMemoryShmemAllocator() {
}

scoped_ptr<DiscardableMemoryShmemChunk>
TestDiscardableMemoryShmemAllocator::AllocateLockedDiscardableMemory(
    size_t size) {
  return make_scoped_ptr(new DiscardableMemoryShmemChunkImpl(size));
}

}  // namespace base
