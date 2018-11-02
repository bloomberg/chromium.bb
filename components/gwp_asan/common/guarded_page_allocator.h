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
#include "components/gwp_asan/common/export.h"

namespace gwp_asan {
namespace internal {

// Method to count trailing zero bits in a uint64_t (identical to
// base::bits::CountTrailingZeroBits64 except that it also works on 32-bit
// platforms.)
unsigned CountTrailingZeroBits64(uint64_t x);

class GWP_ASAN_EXPORT GuardedPageAllocator {
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

  // Initialize the singleton. Used to configure the allocator to map memory
  // for num_pages pages (excluding guard pages). num_pages must be in the range
  // [1, kGpaMaxPages].
  static GuardedPageAllocator& InitializeSingleton(size_t num_pages);

  // Returns the global allocator singleton.
  static GuardedPageAllocator& Get();

  // On success, returns a pointer to size bytes of page-guarded memory. On
  // failure, returns nullptr. The allocation is not guaranteed to be
  // zero-filled. Failure can occur if memory could not be mapped or protected,
  // or if all guarded pages are already allocated.
  //
  // The align parameter specifies a power of two to align the allocation up to.
  // It must be less than or equal to the allocation size. If it's left as zero
  // it will default to the default alignment the allocator chooses.
  //
  // Precondition: align <= size <= page_size_
  void* Allocate(size_t size, size_t align = 0);

  // Deallocates memory pointed to by ptr. ptr must have been previously
  // returned by a call to Allocate.
  void Deallocate(void* ptr);

  // Returns the size requested when ptr was allocated. ptr must have been
  // previously returned by a call to Allocate.
  size_t GetRequestedSize(const void* ptr) const;

  // Returns true if ptr points to memory managed by this class.
  bool PointerIsMine(const void* ptr) const;

 private:
  using BitMap = uint64_t;

  // Structure for storing data about a slot.
  struct SlotMetadata {
    SlotMetadata();
    ~SlotMetadata();

    // Allocate internal data (StackTraces) for this slot. StackTrace objects
    // are large so we only allocate them if they're required (instead of
    // having them be statically allocated in the SlotMetadata itself.)
    void Init();

    // Update slot metadata on an allocation with the given size and offset.
    void RecordAllocation(size_t size, size_t offset);

    // Update slot metadata on a deallocation.
    void RecordDeallocation();

    // Size of the allocation
    size_t alloc_size = 0;
    // How far into the page is the returned allocation.
    size_t alloc_offset = 0;

    // (De)allocation thread id or base::kInvalidThreadId if no (de)allocation
    // occurred.
    base::PlatformThreadId alloc_tid = base::kInvalidThreadId;
    base::PlatformThreadId dealloc_tid = base::kInvalidThreadId;

    // Pointer to stack trace addresses or null if no (de)allocation occurred.
    const void* const* alloc_trace_addr = nullptr;
    const void* const* dealloc_trace_addr = nullptr;

    // Stack trace length or 0 if no (de)allocation occurred.
    size_t alloc_trace_len = 0;
    size_t dealloc_trace_len = 0;

   private:
    // Call destructors on stacktrace_alloc and stacktrace_dealloc if
    // constructors for them have previously been called.
    void Reset();

    // StackTrace objects for this slot, they are allocated by Init() and only
    // used internally, (de)alloc_trace_addr/len should be used by external
    // consumers of the stack trace data.
    base::debug::StackTrace* stacktrace_alloc = nullptr;
    base::debug::StackTrace* stacktrace_dealloc = nullptr;
  };

  // Number of bits in the free_pages_ bitmap.
  static constexpr size_t kFreePagesNumBits = sizeof(BitMap) * 8;

  // Configures this allocator to map memory for num_pages pages (excluding
  // guard pages). num_pages must be in the range [1, kGpaMaxPages].
  //
  // Marked private so that the singleton Get() method is the only way to obtain
  // an instance.
  explicit GuardedPageAllocator(size_t num_pages);

  // Unmaps memory allocated by this class.
  //
  // This method should be called only once to complete destruction.
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
  SlotMetadata data_[kFreePagesNumBits] = {};

  uintptr_t pages_base_addr_ = 0;  // Points to start of mapped region.
  uintptr_t pages_end_addr_ = 0;   // Points to the end of mapped region.
  uintptr_t first_page_addr_ = 0;  // Points to first allocatable page.
  size_t num_pages_ = 0;  // Number of pages mapped (excluding guard pages).
  size_t page_size_ = 0;  // Page size.

  // Set to true if a double free has occurred.
  std::atomic<bool> double_free_detected_{false};

  // Required to access the constructor in Get().
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
