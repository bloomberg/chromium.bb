// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_discardable_shared_memory_manager.h"

#include "base/memory/discardable_shared_memory.h"
#include "content/child/child_thread.h"
#include "content/common/child_process_messages.h"

namespace content {

ChildDiscardableSharedMemoryManager::ChildDiscardableSharedMemoryManager(
    ThreadSafeSender* sender)
    : sender_(sender) {
}

ChildDiscardableSharedMemoryManager::~ChildDiscardableSharedMemoryManager() {
}

scoped_ptr<base::DiscardableSharedMemory>
ChildDiscardableSharedMemoryManager::AllocateLockedDiscardableSharedMemory(
    size_t size) {
  TRACE_EVENT1("renderer",
               "ChildDiscardableSharedMemoryManager::"
               "AllocateLockedDiscardableSharedMemory",
               "size",
               size);

  base::SharedMemoryHandle handle = base::SharedMemory::NULLHandle();
  sender_->Send(
      new ChildProcessHostMsg_SyncAllocateLockedDiscardableSharedMemory(
          size, &handle));
  scoped_ptr<base::DiscardableSharedMemory> memory(
      new base::DiscardableSharedMemory(handle));
  CHECK(memory->Map(size));
  return memory.Pass();
}

}  // namespace content
