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
class DiscardableMemory;
}  // namespace base

#if defined(COMPILER_GCC)
namespace BASE_HASH_NAMESPACE {
template <>
struct hash<const base::DiscardableMemory*> {
  size_t operator()(const base::DiscardableMemory* ptr) const {
    return hash<size_t>()(reinterpret_cast<size_t>(ptr));
  }
};
}  // namespace BASE_HASH_NAMESPACE
#endif  // COMPILER

namespace base {
namespace internal {

// The DiscardableMemoryManager manages a collection of emulated
// DiscardableMemory instances. It is used on platforms that do not support
// discardable memory natively. It keeps track of all DiscardableMemory
// instances (in case they need to be purged), and the total amount of
// allocated memory (in case this forces a purge).
//
// When notified of memory pressure, the manager either purges the LRU
// memory -- if the pressure is moderate -- or all discardable memory
// if the pressure is critical.
//
// NB - this class is an implementation detail. It has been exposed for testing
// purposes. You should not need to use this class directly.
class BASE_EXPORT_PRIVATE DiscardableMemoryManager {
 public:
  DiscardableMemoryManager();
  ~DiscardableMemoryManager();

  // Call this to register memory pressure listener. Must be called on a
  // thread with a MessageLoop current.
  void RegisterMemoryPressureListener();

  // Call this to unregister memory pressure listener.
  void UnregisterMemoryPressureListener();

  // The maximum number of bytes of discardable memory that may be allocated
  // before we force a purge. If this amount is zero, it is interpreted as
  // having no limit at all.
  void SetDiscardableMemoryLimit(size_t bytes);

  // Sets the amount of memory to keep when we're under moderate pressure.
  void SetBytesToKeepUnderModeratePressure(size_t bytes);

  // Adds the given discardable memory to the manager's collection.
  void Register(const DiscardableMemory* discardable, size_t bytes);

  // Removes the given discardable memory from the manager's collection.
  void Unregister(const DiscardableMemory* discardable);

  // Returns NULL if an error occurred. Otherwise, returns the backing buffer
  // and sets |purged| to indicate whether or not the backing buffer has been
  // purged since last use.
  scoped_ptr<uint8, FreeDeleter> Acquire(
      const DiscardableMemory* discardable, bool* purged);

  // Release a previously acquired backing buffer. This gives the buffer back
  // to the manager where it can be purged if necessary.
  void Release(const DiscardableMemory* discardable,
               scoped_ptr<uint8, FreeDeleter> memory);

  // Purges all discardable memory.
  void PurgeAll();

  // Returns true if discardable memory has been added to the manager's
  // collection. This should only be used by tests.
  bool IsRegisteredForTest(const DiscardableMemory* discardable) const;

  // Returns true if discardable memory can be purged. This should only
  // be used by tests.
  bool CanBePurgedForTest(const DiscardableMemory* discardable) const;

  // Returns total amount of allocated discardable memory. This should only
  // be used by tests.
  size_t GetBytesAllocatedForTest() const;

 private:
  struct Allocation {
   explicit Allocation(size_t bytes)
       : bytes(bytes),
         memory(NULL) {
   }

    size_t bytes;
    uint8* memory;
  };
  typedef HashingMRUCache<const DiscardableMemory*, Allocation> AllocationMap;

  // This can be called as a hint that the system is under memory pressure.
  void OnMemoryPressure(
      MemoryPressureListener::MemoryPressureLevel pressure_level);

  // Purges until discardable memory usage is within
  // |bytes_to_keep_under_moderate_pressure_|.
  void Purge();

  // Purges least recently used memory until usage is less or equal to |limit|.
  // Caller must acquire |lock_| prior to calling this function.
  void PurgeLRUWithLockAcquiredUntilUsageIsWithin(size_t limit);

  // Ensures that we don't allocate beyond our memory limit.
  // Caller must acquire |lock_| prior to calling this function.
  void EnforcePolicyWithLockAcquired();

  // Called when a change to |bytes_allocated_| has been made.
  void BytesAllocatedChanged() const;

  // Needs to be held when accessing members.
  mutable Lock lock_;

  // A MRU cache of all allocated bits of discardable memory. Used for purging.
  AllocationMap allocations_;

  // The total amount of allocated discardable memory.
  size_t bytes_allocated_;

  // The maximum number of bytes of discardable memory that may be allocated.
  size_t discardable_memory_limit_;

  // Under moderate memory pressure, we will purge until usage is within this
  // limit.
  size_t bytes_to_keep_under_moderate_pressure_;

  // Allows us to be respond when the system reports that it is under memory
  // pressure.
  scoped_ptr<MemoryPressureListener> memory_pressure_listener_;

  DISALLOW_COPY_AND_ASSIGN(DiscardableMemoryManager);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_MEMORY_DISCARDABLE_MEMORY_MANAGER_H_
