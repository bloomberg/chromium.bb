// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
#define COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_

#include <atomic>
#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/common/allocator_state.h"

namespace gwp_asan {
namespace internal {

// This class encompasses the allocation and deallocation logic on top of the
// AllocatorState. Its members are not inspected or used by the crash handler.
class GWP_ASAN_EXPORT GuardedPageAllocator {
 public:
  // Default maximum alignment for all returned allocations.
  static constexpr size_t kGpaAllocAlignment = 16;

  // Does not allocate any memory for the allocator, to finish initializing call
  // Init().
  GuardedPageAllocator();

  // Configures this allocator to allocate up to max_alloced_pages pages at a
  // time from a pool of total_pages pages, where:
  //   1 <= max_alloced_pages <= num_metadata == total_pages <= kMaxSlots
  void Init(size_t max_alloced_pages, size_t num_metadata, size_t total_pages);

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // or if all guarded pages are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // Preconditions: Init() must have been called.
  void* Allocate(size_t size, size_t align = 0);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate, and not have been deallocated.
  size_t GetRequestedSize(const void* ptr) const;

  // Get the address of the GuardedPageAllocator crash key (the address of the
  // the shared allocator state with the crash handler.)
  uintptr_t GetCrashKeyAddress() const;

  // Returns true if ptr points to memory managed by this class.
  inline bool PointerIsMine(const void* ptr) const {
    return state_.PointerIsMine(reinterpret_cast<uintptr_t>(ptr));
  }

 private:
  // Unmaps memory allocated by this class, if Init was called.
  ~GuardedPageAllocator();

  // Allocates/deallocates the virtual memory used for allocations.
  void* MapRegion();
  void UnmapRegion();

  // Returns the size of the virtual memory region used to store allocations.
  size_t RegionSize() const;

  // Mark page read-write.
  void MarkPageReadWrite(void*);

  // Mark page inaccessible and decommit the memory from use to save memory
  // used by the quarantine.
  void MarkPageInaccessible(void*);

  // On success, returns true and writes the reserved indices to |slot| and
  // |metadata_idx|. Otherwise returns false if no allocations are available.
  bool ReserveSlotAndMetadata(AllocatorState::SlotIdx* slot,
                              AllocatorState::MetadataIdx* metadata_idx)
      LOCKS_EXCLUDED(lock_);

  // Marks the specified slot and metadata as unreserved.
  void FreeSlotAndMetadata(AllocatorState::SlotIdx slot,
                           AllocatorState::MetadataIdx metadata_idx)
      LOCKS_EXCLUDED(lock_);

  // Record the metadata for an allocation or deallocation for a given metadata
  // index.
  ALWAYS_INLINE
  void RecordAllocationMetadata(AllocatorState::MetadataIdx metadata_idx,
                                size_t size,
                                void* ptr);
  ALWAYS_INLINE void RecordDeallocationMetadata(
      AllocatorState::MetadataIdx metadata_idx);

  // Allocator state shared with with the crash analyzer.
  AllocatorState state_;

  // Lock that synchronizes allocating/freeing slots between threads.
  base::Lock lock_;

  // Fixed-size array used to store all free slot indices.
  AllocatorState::SlotIdx free_slots_[AllocatorState::kMaxSlots] GUARDED_BY(
      lock_);
  // Stores the end index of the array.
  size_t free_slots_end_ GUARDED_BY(lock_) = 0;

  // Number of currently-allocated pages.
  size_t num_alloced_pages_ GUARDED_BY(lock_) = 0;
  // Max number of concurrent allocations.
  size_t max_alloced_pages_ = 0;

  // We dynamically allocate the SlotMetadata array to avoid allocating
  // extraneous memory for when num_metadata < kMaxMetadata.
  std::unique_ptr<AllocatorState::SlotMetadata[]> metadata_;

  // Required for a singleton to access the constructor.
  friend base::NoDestructor<GuardedPageAllocator>;

  friend class GuardedPageAllocatorTest;
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest,
                           GetNearestValidPageEdgeCases);
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest, GetErrorTypeEdgeCases);

  DISALLOW_COPY_AND_ASSIGN(GuardedPageAllocator);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_CLIENT_GUARDED_PAGE_ALLOCATOR_H_
