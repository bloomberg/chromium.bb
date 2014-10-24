// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include "base/memory/discardable_memory_shmem_allocator.h"
#include "base/memory/ref_counted.h"
#include "content/child/thread_safe_sender.h"

namespace content {

// Implementation of DiscardableMemoryShmemAllocator that allocates
// discardable memory segments through the browser process.
class ChildDiscardableSharedMemoryManager
    : public base::DiscardableMemoryShmemAllocator {
 public:
  explicit ChildDiscardableSharedMemoryManager(ThreadSafeSender* sender);
  ~ChildDiscardableSharedMemoryManager() override;

  // Overridden from base::DiscardableMemoryShmemAllocator:
  scoped_ptr<base::DiscardableSharedMemory>
  AllocateLockedDiscardableSharedMemory(size_t size) override;

 private:
  scoped_refptr<ThreadSafeSender> sender_;

  DISALLOW_COPY_AND_ASSIGN(ChildDiscardableSharedMemoryManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
