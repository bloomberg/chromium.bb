// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_

#include "base/base_export.h"
#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/synchronization/lock.h"

namespace base {
namespace internal {

// This interface is used by the DiscardableMemoryManager class to provide some
// level of userspace control over discardable memory allocations.
class DiscardableMemoryManagerAllocation {
 public:
  // Allocate and acquire a lock that prevents the allocation from being purged
  // by the system. Returns true if memory was previously allocated and is still
  // resident.
  virtual bool AllocateAndAcquireLock() = 0;

  // Release a previously acquired lock on the allocation so that it can be
  // purged by the system.
  virtual void ReleaseLock() = 0;

  // Explicitly purge this allocation. It is illegal to call this while a lock
  // is acquired on the allocation.
  virtual void Purge() = 0;

 protected:
  virtual ~DiscardableMemoryManagerAllocation() {}
};

}  // namespace internal
}  // namespace base

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<base::internal::DiscardableMemoryManagerAllocation*> {
  size_t operator()(
      base::internal::DiscardableMemoryManagerAllocation* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace base {
namespace internal {

// The DiscardableMemoryManager manages a collection of
// DiscardableMemoryManagerAllocation instances. It is used on platforms that
// need some level of userspace control over discardable memory. It keeps track
// of all allocation instances (in case they need to be purged), and the total
// amount of allocated memory (in case this forces a purge). When memory usage
// reaches the limit, the manager purges the LRU memory.
//
// When notified of memory pressure, the manager either purges the LRU memory --
// if the pressure is moderate -- or all discardable memory if the pressure is
// critical.
class BASE_EXPORT_PRIVATE DiscardableMemoryManager {
 public:
  typedef DiscardableMemoryManagerAllocation Allocation;

  DiscardableMemoryManager();
  ~DiscardableMemoryManager();

  // Call this to register memory pressure listener. Must be called on a thread
  // with a MessageLoop current.
  void RegisterMemoryPressureListener();

  // Call this to unregister memory pressure listener.
  void UnregisterMemoryPressureListener();

  // The maximum number of bytes of memory that may be allocated before we force
  // a purge. If this amount is zero, it is interpreted as having no limit at
  // all.
  void SetMemoryLimit(size_t bytes);

  // Sets the amount of memory to keep when we're under moderate pressure.
  void SetBytesToKeepUnderModeratePressure(size_t bytes);

  // Adds the given allocation to the manager's collection.
  void Register(Allocation* allocation, size_t bytes);

  // Removes the given allocation from the manager's collection.
  void Unregister(Allocation* allocation);

  // Returns false if an error occurred. Otherwise, returns true and sets
  // |purged| to indicate whether or not allocation has been purged since last
  // use.
  bool AcquireLock(Allocation* allocation, bool* purged);

  // Release a previously acquired lock on allocation. This allows the manager
  // to purge it if necessary.
  void ReleaseLock(Allocation* allocation);

  // Purges all discardable memory.
  void PurgeAll();

  // Returns true if allocation has been added to the manager's collection. This
  // should only be used by tests.
  bool IsRegisteredForTest(Allocation* allocation) const;

  // Returns true if allocation can be purged. This should only be used by
  // tests.
  bool CanBePurgedForTest(Allocation* allocation) const;

  // Returns total amount of allocated discardable memory. This should only be
  // used by tests.
  size_t GetBytesAllocatedForTest() const;

 private:
  struct AllocationInfo {
    explicit AllocationInfo(size_t bytes) : bytes(bytes), purgable(false) {}

    const size_t bytes;
    bool purgable;
  };
  typedef HashingMRUCache<Allocation*, AllocationInfo> AllocationMap;

  // This can be called as a hint that the system is under memory pressure.
  void OnMemoryPressure(
      MemoryPressureListener::MemoryPressureLevel pressure_level);

  // Purges memory until usage is within
  // |bytes_to_keep_under_moderate_pressure_|.
  void Purge();

  // Purges least recently used memory until usage is less or equal to |limit|.
  // Caller must acquire |lock_| prior to calling this function.
  void PurgeLRUWithLockAcquiredUntilUsageIsWithin(size_t limit);

  // Ensures that we don't allocate beyond our memory limit. Caller must acquire
  // |lock_| prior to calling this function.
  void EnforcePolicyWithLockAcquired();

  // Called when a change to |bytes_allocated_| has been made.
  void BytesAllocatedChanged() const;

  // Needs to be held when accessing members.
  mutable Lock lock_;

  // A MRU cache of all allocated bits of memory. Used for purging.
  AllocationMap allocations_;

  // The total amount of allocated memory.
  size_t bytes_allocated_;

  // The maximum number of bytes of memory that may be allocated.
  size_t memory_limit_;

  // Under moderate memory pressure, we will purge memory until usage is within
  // this limit.
  size_t bytes_to_keep_under_moderate_pressure_;

  // Allows us to be respond when the system reports that it is under memory
  // pressure.
  scoped_ptr<MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
