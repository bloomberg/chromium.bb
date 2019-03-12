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
  //   1 <= max_alloced_pages <= total_pages <= kGpaMaxPages
  void Init(size_t max_alloced_pages, size_t total_pages);

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // or if all guarded pages are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // Preconditions: Init() must have been called,
  //                size <= page_size,
  //                align <= page_size
  void* Allocate(size_t size, size_t align = 0);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate.
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

  // On success, returns true and writes the reserved slot to |slot|.
  // Otherwise returns false if no slots are available.
  bool ReserveSlot(AllocatorState::SlotIdx* slot) LOCKS_EXCLUDED(lock_);

  // Marks the specified slot as unreserved.
  void FreeSlot(AllocatorState::SlotIdx slot) LOCKS_EXCLUDED(lock_);

  // Record an allocation or deallocation for a given slot index. This
  // encapsulates the logic for updating the stack traces and metadata for a
  // given slot.
  ALWAYS_INLINE
  void RecordAllocationMetadata(AllocatorState::SlotIdx slot,
                                size_t size,
                                void* ptr);
  ALWAYS_INLINE void RecordDeallocationMetadata(AllocatorState::SlotIdx slot);

  // Allocator state shared with with the crash analyzer.
  AllocatorState state_;

  // Lock that synchronizes allocating/freeing slots between threads.
  base::Lock lock_;

  // Fixed-size array used to store all free slot indices.
  AllocatorState::SlotIdx free_slots_[AllocatorState::kGpaMaxPages] GUARDED_BY(
      lock_);
  // Stores the end index of the array.
  size_t free_slots_end_ GUARDED_BY(lock_) = 0;

  // Number of currently-allocated pages.
  size_t num_alloced_pages_ GUARDED_BY(lock_) = 0;
  // Max number of pages to allocate at once.
  size_t max_alloced_pages_ = 0;

  // We dynamically allocate the SlotMetadata array to avoid allocating
  // extraneous memory for when total_pages < kGpaMaxPages.
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
