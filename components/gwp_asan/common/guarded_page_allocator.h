// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_GWP_ASAN_COMMON_GUARDED_PAGE_ALLOCATOR_H_
#define COMPONENTS_GWP_ASAN_COMMON_GUARDED_PAGE_ALLOCATOR_H_

#include <atomic>

#include "base/debug/stack_trace.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"

namespace gwp_asan {
namespace internal {

// Method to count trailing zero bits in a uint64_t (identical to
// base::bits::CountTrailingZeroBits64 except that it also works on 32-bit
// platforms.)
unsigned CountTrailingZeroBits64(uint64_t x);

class GuardedPageAllocator {
 public:
  // Maximum number of pages this class can allocate.
  static constexpr size_t kGpaMaxPages = 64;

  // Default maximum alignment for all returned allocations.
  static constexpr size_t kGpaAllocAlignment = 16;

  enum class ErrorType {
    kUseAfterFree = 0,
    kBufferUnderflow = 1,
    kBufferOverflow = 2,
    kDoubleFree = 3,
    kUnknown = 4,
  };

  // Configures this allocator to map memory for num_pages pages (excluding
  // guard pages). num_pages must be in the range [1, kGpaMaxPages]. Init should
  // only be called once.
  void Init(size_t num_pages);

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // if the allocation is greather than a page in size, or if all guarded pages
  // are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // Precondition: Init() must have been called, align <= size
  void* Allocate(size_t size, size_t align = 0);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate.
  size_t GetRequestedSize(const void* ptr) const;

  // Returns true if ptr points to memory managed by this class.
  inline bool PointerIsMine(const void* ptr) const {
    uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
    return pages_base_addr_ <= addr && addr < pages_end_addr_;
  }

 private:
  using BitMap = uint64_t;
  static_assert(kGpaMaxPages == sizeof(BitMap) * 8,
                "Maximum number of pages is the size of free_pages_ bitmap");

  // Structure for storing data about a slot.
  struct SlotMetadata {
    // Information saved for allocations and deallocations.
    struct AllocationInfo {
      // (De)allocation thread id or base::kInvalidThreadId if no (de)allocation
      // occurred.
      base::PlatformThreadId tid = base::kInvalidThreadId;

      // Pointer to stack trace addresses or null if no (de)allocation occurred.
      const void* const* trace_addr = nullptr;

      // Stack trace length or 0 if no (de)allocation occurred.
      size_t trace_len = 0;

     private:
      // StackTrace object for this slot, it's allocated in
      // SlotMetadata()::Init() and only used internally, trace_addr/len should
      // be used by external consumers of the stack trace data.
      base::debug::StackTrace* stacktrace = nullptr;

      friend struct SlotMetadata;
    };

    // Allocate internal data (StackTraces) for this slot. StackTrace objects
    // are large so we only allocate them if they're required (instead of
    // having them be statically allocated in the SlotMetadata itself.)
    void Init();

    // Frees internal data. May only be called after Init().
    void Destroy();

    // Update slot metadata on an allocation with the given size and pointer.
    void RecordAllocation(size_t size, void* ptr);

    // Update slot metadata on a deallocation.
    void RecordDeallocation();

    // Size of the allocation
    size_t alloc_size = 0;
    // The allocation address.
    uintptr_t alloc_ptr = 0;

    AllocationInfo alloc;
    AllocationInfo dealloc;

   private:
    // Call destructors on (de)alloc.stacktrace if constructors for them have
    // previously been called.
    void Reset();
  };

  // Does not allocate any memory for the allocator, to finish initializing call
  // Init().
  GuardedPageAllocator();

  // Unmaps memory allocated by this class, if Init was called.
  ~GuardedPageAllocator();

  // Maps pages into memory and sets pages_base_addr_, first_page_addr_, and
  // pages_end_addr on success. Returns true on success, false on failure.
  bool MapPages();

  // Unmaps pages.
  void UnmapPages();

  // Mark page read-write or inaccessible.
  void MarkPageReadWrite(void*);
  void MarkPageInaccessible(void*);

  // Reserves and returns a slot randomly selected from the free slots in
  // free_pages_. Returns SIZE_MAX if no slots available.
  size_t ReserveSlot() LOCKS_EXCLUDED(lock_);

  // Marks the specified slot as unreserved.
  void FreeSlot(size_t slot) LOCKS_EXCLUDED(lock_);

  // Returns the address of the page that addr resides on.
  uintptr_t GetPageAddr(uintptr_t addr) const;

  // Returns an address somewhere on the valid page nearest to addr.
  uintptr_t GetNearestValidPage(uintptr_t addr) const;

  // Returns the slot number for the page nearest to addr.
  size_t GetNearestSlot(uintptr_t addr) const;

  // Returns the likely error type given an exception address and whether its
  // previously been allocated and deallocated.
  ErrorType GetErrorType(uintptr_t addr,
                         bool allocated,
                         bool deallocated) const;

  uintptr_t SlotToAddr(size_t slot) const;
  size_t AddrToSlot(uintptr_t addr) const;

  // Allocator lock that protects free_pages_.
  base::Lock lock_;

  // Maps each bit to one page.
  // Bit=1: Free. Bit=0: Reserved.
  BitMap free_pages_ GUARDED_BY(lock_) = 0;

  // Information about every allocation, including its size, offset, and
  // pointers to the allocation/deallocation stack traces (if present.)
  SlotMetadata data_[kGpaMaxPages] = {};

  uintptr_t pages_base_addr_ = 0;  // Points to start of mapped region.
  uintptr_t pages_end_addr_ = 0;   // Points to the end of mapped region.
  uintptr_t first_page_addr_ = 0;  // Points to first allocatable page.
  size_t num_pages_ = 0;  // Number of pages mapped (excluding guard pages).
  size_t page_size_ = 0;  // Page size.

  // Set to true if a double free has occurred.
  std::atomic<bool> double_free_detected_{false};

  // Required for a singleton to access the constructor.
  friend base::NoDestructor<GuardedPageAllocator>;

  DISALLOW_COPY_AND_ASSIGN(GuardedPageAllocator);

  friend class GuardedPageAllocatorTest;
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest,
                           GetNearestValidPageEdgeCases);
  FRIEND_TEST_ALL_PREFIXES(GuardedPageAllocatorTest, GetErrorTypeEdgeCases);
};

}  // namespace internal
}  // namespace gwp_asan

#endif  // COMPONENTS_GWP_ASAN_COMMON_GUARDED_PAGE_ALLOCATOR_H_
