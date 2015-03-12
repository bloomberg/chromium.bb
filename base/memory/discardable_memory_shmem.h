// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_H_

#include "base/memory/discardable_memory.h"

namespace base {
class DiscardableMemoryShmemChunk;

namespace internal {

class DiscardableMemoryShmem : public DiscardableMemory {
 public:
  explicit DiscardableMemoryShmem(size_t bytes);
  ~DiscardableMemoryShmem() override;

  bool Initialize();

  // Overridden from DiscardableMemory:
  bool Lock() override;
  void Unlock() override;
  void* Memory() const override;

 private:
  scoped_ptr<DiscardableMemoryShmemChunk> chunk_;
  bool is_locked_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryShmem);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_SHMEM_H_
