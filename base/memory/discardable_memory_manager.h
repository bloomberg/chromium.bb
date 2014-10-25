// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
#define BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_

#include "base/base_export.h"
#include "base/containers/hash_tables.h"
#include "base/containers/mru_cache.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

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

  // Check if allocated memory is still resident. It is illegal to call this
  // while a lock is acquired on the allocation.
  virtual bool IsMemoryResident() const = 0;

 protected:
  virtual ~DiscardableMemoryManagerAllocation() {}
};

}  // namespace internal
}  // namespace base

namespace base {
namespace internal {

// The DiscardableMemoryManager manages a collection of
// DiscardableMemoryManagerAllocation instances. It is used on platforms that
// need some level of userspace control over discardable memory. It keeps track
// of all allocation instances (in case they need to be purged), and the total
// amount of allocated memory (in case this forces a purge). When memory usage
// reaches the limit, the manager purges the LRU memory.
class BASE_EXPORT_PRIVATE DiscardableMemoryManager {
 public:
  typedef DiscardableMemoryManagerAllocation Allocation;

  DiscardableMemoryManager(size_t memory_limit,
                           size_t soft_memory_limit,
                           TimeDelta hard_memory_limit_expiration_time);
  virtual ~DiscardableMemoryManager();

  // The maximum number of bytes of memory that may be allocated before we force
  // a purge.
  void SetMemoryLimit(size_t bytes);

  // The number of bytes of memory that may be allocated but unused for the hard
  // limit expiration time without getting purged.
  void SetSoftMemoryLimit(size_t bytes);

  // Sets the memory usage cutoff time for hard memory limit.
  void SetHardMemoryLimitExpirationTime(
      TimeDelta hard_memory_limit_expiration_time);

  // This will make sure that all purged memory is released to the OS.
  void ReleaseFreeMemory();

  // This will attempt to reduce memory footprint until within soft memory
  // limit. Returns true if there's no need to call this again until allocations
  // have been used.
  bool ReduceMemoryUsage();

  // This can be called to attempt to reduce memory footprint until within
  // limit for bytes to keep under moderate pressure.
  void ReduceMemoryUsageUntilWithinLimit(size_t bytes);

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
    TimeTicks last_usage;
  };
  typedef HashingMRUCache<Allocation*, AllocationInfo> AllocationMap;

  // Purges memory not used since |hard_memory_limit_expiration_time_| before
  // "right now" until usage is less or equal to |soft_memory_limit_|.
  // Returns true if total amount of memory is less or equal to soft memory
  // limit.
  bool PurgeIfNotUsedSinceHardLimitCutoffUntilWithinSoftMemoryLimit();

  // Purges memory that has not been used since |timestamp| until usage is less
  // or equal to |limit|.
  // Caller must acquire |lock_| prior to calling this function.
  void PurgeIfNotUsedSinceTimestampUntilUsageIsWithinLimitWithLockAcquired(
      TimeTicks timestamp,
      size_t limit);

  // Called when a change to |bytes_allocated_| has been made.
  void BytesAllocatedChanged(size_t new_bytes_allocated) const;

  // Virtual for tests.
  virtual TimeTicks Now() const;

  // Needs to be held when accessing members.
  mutable Lock lock_;

  // A MRU cache of all allocated bits of memory. Used for purging.
  AllocationMap allocations_;

  // The total amount of allocated memory.
  size_t bytes_allocated_;

  // The maximum number of bytes of memory that may be allocated.
  size_t memory_limit_;

  // The number of bytes of memory that may be allocated but not used for
  // |hard_memory_limit_expiration_time_| amount of time when receiving an idle
  // notification.
  size_t soft_memory_limit_;

  // Amount of time it takes for an allocation to become affected by
  // |soft_memory_limit_|.
  TimeDelta hard_memory_limit_expiration_time_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
