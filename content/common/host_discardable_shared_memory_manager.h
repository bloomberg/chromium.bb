// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_HOST_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
#define CONTENT_COMMON_HOST_DISCARDABLE_SHARED_MEMORY_MANAGER_H_

#include <vector>

#include "base/memory/discardable_memory_shmem_allocator.h"
#include "base/memory/discardable_shared_memory.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/shared_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"

namespace content {

// Implementation of DiscardableMemoryShmemAllocator that allocates and
// manages discardable memory segments for the browser process and child
// processes. This class is thread-safe and instances can safely be used
// on any thread.
class CONTENT_EXPORT HostDiscardableSharedMemoryManager
    : public base::DiscardableMemoryShmemAllocator {
 public:
  HostDiscardableSharedMemoryManager();
  ~HostDiscardableSharedMemoryManager() override;

  // Returns a singleton instance.
  static HostDiscardableSharedMemoryManager* current();

  // Overridden from base::DiscardableMemoryShmemAllocator:
  scoped_ptr<base::DiscardableMemoryShmemChunk> AllocateLockedDiscardableMemory(
      size_t size) override;

  // This allocates a discardable memory segment for |process_handle|.
  // A valid shared memory handle is returned on success.
  void AllocateLockedDiscardableSharedMemoryForChild(
      base::ProcessHandle process_handle,
      size_t size,
      base::SharedMemoryHandle* shared_memory_handle);

  // Call this to notify the manager that child process associated with
  // |process_handle| has been removed. The manager will use this to release
  // memory segments allocated for child process to the OS.
  void ProcessRemoved(base::ProcessHandle process_handle);

  // The maximum number of bytes of memory that may be allocated. This will
  // cause memory usage to be reduced if currently above |limit|.
  void SetMemoryLimit(size_t limit);

  // Reduce memory usage if above current memory limit.
  void EnforceMemoryPolicy();

 private:
  struct MemorySegment {
    MemorySegment(linked_ptr<base::DiscardableSharedMemory> memory,
                  base::ProcessHandle process_handle);
    ~MemorySegment();

    linked_ptr<base::DiscardableSharedMemory> memory;
    base::ProcessHandle process_handle;
  };

  static bool CompareMemoryUsageTime(const MemorySegment& a,
                                     const MemorySegment& b) {
    // In this system, LRU memory segment is evicted first.
    return a.memory->last_known_usage() > b.memory->last_known_usage();
  }

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level);
  void ReduceMemoryUsageUntilWithinMemoryLimit();
  void ReduceMemoryUsageUntilWithinLimit(size_t limit);
  void BytesAllocatedChanged(size_t new_bytes_allocated) const;

  // Virtual for tests.
  virtual base::Time Now() const;
  virtual void ScheduleEnforceMemoryPolicy();

  base::Lock lock_;
  // Note: The elements in |segments_| are arranged in such a way that they form
  // a heap. The LRU memory segment always first.
  typedef std::vector<MemorySegment> MemorySegmentVector;
  MemorySegmentVector segments_;
  size_t memory_limit_;
  size_t bytes_allocated_;
  scoped_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  bool enforce_memory_policy_pending_;
  base::WeakPtrFactory<HostDiscardableSharedMemoryManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(HostDiscardableSharedMemoryManager);
};

}  // namespace content

#endif  // CONTENT_COMMON_HOST_DISCARDABLE_SHARED_MEMORY_MANAGER_H_
