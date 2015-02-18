// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/child_discardable_shared_memory_manager.h"

#include "base/memory/discardable_shared_memory.h"
#include "base/process/process_metrics.h"
#include "content/common/child_process_messages.h"

namespace content {
namespace {

// Default allocation size.
const size_t kAllocationSize = 4 * 1024 * 1024;

class DiscardableMemoryShmemChunkImpl
    : public base::DiscardableMemoryShmemChunk {
 public:
  DiscardableMemoryShmemChunkImpl(
      ChildDiscardableSharedMemoryManager* manager,
      scoped_ptr<DiscardableSharedMemoryHeap::Span> span)
      : manager_(manager), span_(span.Pass()) {}
  ~DiscardableMemoryShmemChunkImpl() override {
    manager_->ReleaseSpan(span_.Pass());
  }

  // Overridden from DiscardableMemoryShmemChunk:
  bool Lock() override { return manager_->LockSpan(span_.get()); }
  void Unlock() override { manager_->UnlockSpan(span_.get()); }
  void* Memory() const override {
    return reinterpret_cast<void*>(span_->start() * base::GetPageSize());
  }

 private:
  ChildDiscardableSharedMemoryManager* manager_;
  scoped_ptr<DiscardableSharedMemoryHeap::Span> span_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryShmemChunkImpl);
};

}  // namespace

ChildDiscardableSharedMemoryManager::ChildDiscardableSharedMemoryManager(
    ThreadSafeSender* sender)
    : heap_(base::GetPageSize()), sender_(sender) {
}

ChildDiscardableSharedMemoryManager::~ChildDiscardableSharedMemoryManager() {
}

scoped_ptr<base::DiscardableMemoryShmemChunk>
ChildDiscardableSharedMemoryManager::AllocateLockedDiscardableMemory(
    size_t size) {
  base::AutoLock lock(lock_);

  DCHECK_NE(size, 0u);

  // Round up to multiple of page size.
  size_t pages = (size + base::GetPageSize() - 1) / base::GetPageSize();

  for (;;) {
    // Search free list for available space.
    scoped_ptr<DiscardableSharedMemoryHeap::Span> free_span =
        heap_.SearchFreeList(pages);
    if (!free_span.get())
      break;

    // Attempt to lock |free_span|. Delete span and search free list again
    // if locking failed.
    if (free_span->shared_memory()->Lock(
            free_span->start() * base::GetPageSize() -
                reinterpret_cast<size_t>(free_span->shared_memory()->memory()),
            free_span->length() * base::GetPageSize()) ==
        base::DiscardableSharedMemory::FAILED) {
      DCHECK(!free_span->shared_memory()->IsMemoryResident());
      // We have to release free memory before |free_span| can be destroyed.
      heap_.ReleaseFreeMemory();
      DCHECK(!free_span->shared_memory());
      continue;
    }

    return make_scoped_ptr(
        new DiscardableMemoryShmemChunkImpl(this, free_span.Pass()));
  }

  // Release free memory and free up the address space. Failing to do this
  // can result in the child process running out of address space.
  heap_.ReleaseFreeMemory();

  size_t pages_to_allocate =
      std::max(kAllocationSize / base::GetPageSize(), pages);
  size_t allocation_size_in_bytes = pages_to_allocate * base::GetPageSize();

  // Ask parent process to allocate a new discardable shared memory segment.
  scoped_ptr<base::DiscardableSharedMemory> shared_memory(
      AllocateLockedDiscardableSharedMemory(allocation_size_in_bytes));

  // Create span for allocated memory.
  scoped_ptr<DiscardableSharedMemoryHeap::Span> new_span(
      heap_.Grow(shared_memory.Pass(), allocation_size_in_bytes));

  // Unlock and insert any left over memory into free list.
  if (pages < pages_to_allocate) {
    scoped_ptr<DiscardableSharedMemoryHeap::Span> leftover =
        heap_.Split(new_span.get(), pages);
    leftover->shared_memory()->Unlock(
        leftover->start() * base::GetPageSize() -
            reinterpret_cast<size_t>(leftover->shared_memory()->memory()),
        leftover->length() * base::GetPageSize());
    heap_.MergeIntoFreeList(leftover.Pass());
  }

  return make_scoped_ptr(
      new DiscardableMemoryShmemChunkImpl(this, new_span.Pass()));
}

void ChildDiscardableSharedMemoryManager::ReleaseFreeMemory() {
  base::AutoLock lock(lock_);

  heap_.ReleaseFreeMemory();
}

bool ChildDiscardableSharedMemoryManager::LockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  if (!span->shared_memory())
    return false;

  size_t offset = span->start() * base::GetPageSize() -
      reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  switch (span->shared_memory()->Lock(offset, length)) {
    case base::DiscardableSharedMemory::SUCCESS:
      return true;
    case base::DiscardableSharedMemory::PURGED:
      span->shared_memory()->Unlock(offset, length);
      return false;
    case base::DiscardableSharedMemory::FAILED:
      return false;
  }

  NOTREACHED();
  return false;
}

void ChildDiscardableSharedMemoryManager::UnlockSpan(
    DiscardableSharedMemoryHeap::Span* span) {
  base::AutoLock lock(lock_);

  DCHECK(span->shared_memory());
  size_t offset = span->start() * base::GetPageSize() -
      reinterpret_cast<size_t>(span->shared_memory()->memory());
  size_t length = span->length() * base::GetPageSize();

  return span->shared_memory()->Unlock(offset, length);
}

void ChildDiscardableSharedMemoryManager::ReleaseSpan(
    scoped_ptr<DiscardableSharedMemoryHeap::Span> span) {
  base::AutoLock lock(lock_);

  // Delete span instead of merging it into free list if memory is gone.
  if (!span->shared_memory())
    return;

  // Purge backing memory if span is greater than kAllocationSize. This will
  // prevent it from being reused and associated resources will be released.
  if (span->length() * base::GetPageSize() > kAllocationSize)
    span->shared_memory()->Purge(base::Time::Now());

  heap_.MergeIntoFreeList(span.Pass());
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
