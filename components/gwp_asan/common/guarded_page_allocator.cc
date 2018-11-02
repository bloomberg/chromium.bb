// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/guarded_page_allocator.h"

#include "base/bits.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/synchronization/lock.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

using base::debug::StackTrace;

namespace gwp_asan {
namespace internal {

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t GuardedPageAllocator::kGpaMaxPages;
constexpr size_t GuardedPageAllocator::kGpaAllocAlignment;
constexpr size_t GuardedPageAllocator::kFreePagesNumBits;

GuardedPageAllocator& GuardedPageAllocator::InitializeSingleton(
    size_t num_pages) {
  static base::NoDestructor<GuardedPageAllocator> gpa(num_pages);
  return *gpa;
}

GuardedPageAllocator& GuardedPageAllocator::Get() {
  // The constructor will fail if it is called with num_pages = 0, forcing
  // InitializeSingleton() to be called first.
  return InitializeSingleton(0);
}

GuardedPageAllocator::GuardedPageAllocator(size_t num_pages) {
  CHECK_GT(num_pages, 0U);
  CHECK_LE(num_pages, kFreePagesNumBits);
  num_pages_ = num_pages;

  page_size_ = base::GetPageSize();
  CHECK(MapPages());

  free_pages_ =
      (num_pages_ == kFreePagesNumBits) ? ~0ULL : (1ULL << num_pages_) - 1;

  for (size_t i = 0; i < num_pages_; i++)
    data_[i].Init();
}

GuardedPageAllocator::~GuardedPageAllocator() {
  UnmapPages();
}

void* GuardedPageAllocator::Allocate(size_t size, size_t align) {
  CHECK_LE(size, page_size_);
  if (!size)
    return nullptr;

  // Default alignment is size's next smallest power-of-two, up to
  // kGpaAllocAlignment.
  if (!align) {
    align =
        std::min(size_t{1} << base::bits::Log2Floor(size), kGpaAllocAlignment);
  }
  CHECK_LE(align, size);
  CHECK(base::bits::IsPowerOfTwo(align));

  size_t free_slot = ReserveSlot();
  if (free_slot == SIZE_MAX)
    return nullptr;  // All slots are reserved.

  uintptr_t free_page = SlotToAddr(free_slot);
  MarkPageReadWrite(reinterpret_cast<void*>(free_page));

  size_t offset;
  if (base::RandInt(0, 1))
    // Return right-aligned allocation to detect overflows.
    offset = page_size_ - base::bits::Align(size, align);
  else
    // Return left-aligned allocation to detect underflows.
    offset = 0;

  // Initialize slot metadata.
  data_[free_slot].RecordAllocation(size, offset);

  return reinterpret_cast<void*>(free_page + offset);
}

void GuardedPageAllocator::Deallocate(void* ptr) {
  CHECK(PointerIsMine(ptr));

  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  MarkPageInaccessible(reinterpret_cast<void*>(GetPageAddr(addr)));

  size_t slot = AddrToSlot(GetPageAddr(addr));
  DCHECK_EQ(addr, GetPageAddr(addr) + data_[slot].alloc_offset);
  // Check for double free.
  if (data_[slot].dealloc_trace_addr) {
    double_free_detected_ = true;
    *reinterpret_cast<char*>(ptr) = 'X';  // Trigger exception.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id.
  data_[slot].RecordDeallocation();

  FreeSlot(slot);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  DCHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  size_t slot = AddrToSlot(GetPageAddr(addr));
  DCHECK_EQ(addr, GetPageAddr(addr) + data_[slot].alloc_offset);
  return data_[slot].alloc_size;
}

bool GuardedPageAllocator::PointerIsMine(const void* ptr) const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  return pages_base_addr_ <= addr && addr < pages_end_addr_;
}

// Selects a random slot in O(1) time by rotating the free_pages bitmap by a
// random amount, using an intrinsic to get the least-significant 1-bit after
// the rotation, and then computing the position of the bit before the rotation.
// Picking a random slot is useful for randomizing allocator behavior across
// different runs, so certain bits being more heavily biased is not a concern.
size_t GuardedPageAllocator::ReserveSlot() {
  base::AutoLock lock(lock_);
  if (!free_pages_)
    return SIZE_MAX;

  // Disable allocations after a double free is detected so that the double
  // freed allocation is not reallocated while the crash handler could be
  // concurrently inspecting the metadata.
  if (double_free_detected_)
    return SIZE_MAX;

  uint64_t rot = base::RandGenerator(kFreePagesNumBits);
  BitMap rotated_bitmap =
      (free_pages_ << rot) | (free_pages_ >> (kFreePagesNumBits - rot));
  int rotated_selection = CountTrailingZeroBits64(rotated_bitmap);
  size_t selection =
      (rotated_selection - rot + kFreePagesNumBits) % kFreePagesNumBits;
  DCHECK_LT(selection, kFreePagesNumBits);
  DCHECK(free_pages_ & (1ULL << selection));
  free_pages_ &= ~(1ULL << selection);
  return selection;
}

void GuardedPageAllocator::FreeSlot(size_t slot) {
  DCHECK_LT(slot, kFreePagesNumBits);

  BitMap bit = 1ULL << slot;
  base::AutoLock lock(lock_);
  DCHECK_EQ((free_pages_ & bit), 0ULL);
  free_pages_ |= bit;
}

uintptr_t GuardedPageAllocator::GetPageAddr(uintptr_t addr) const {
  const uintptr_t addr_mask = ~(page_size_ - 1ULL);
  return addr & addr_mask;
}

uintptr_t GuardedPageAllocator::GetNearestValidPage(uintptr_t addr) const {
  if (addr < first_page_addr_)
    return first_page_addr_;
  const uintptr_t last_page_addr = pages_end_addr_ - 2 * page_size_;
  if (addr > last_page_addr)
    return last_page_addr;

  uintptr_t offset = addr - first_page_addr_;
  // If addr is already on a valid page, just return addr.
  if ((offset / page_size_) % 2 == 0)
    return addr;

  // ptr points to a guard page, so get nearest valid page.
  const size_t kHalfPageSize = page_size_ / 2;
  if ((offset / kHalfPageSize) % 2 == 0) {
    return addr - kHalfPageSize;  // Round down.
  }
  return addr + kHalfPageSize;  // Round up.
}

size_t GuardedPageAllocator::GetNearestSlot(uintptr_t addr) const {
  return AddrToSlot(GetPageAddr(GetNearestValidPage(addr)));
}

GuardedPageAllocator::ErrorType GuardedPageAllocator::GetErrorType(
    uintptr_t addr,
    bool allocated,
    bool deallocated) const {
  if (!allocated)
    return ErrorType::kUnknown;
  if (double_free_detected_)
    return ErrorType::kDoubleFree;
  if (deallocated)
    return ErrorType::kUseAfterFree;
  if (addr < first_page_addr_)
    return ErrorType::kBufferUnderflow;
  const uintptr_t last_page_addr = pages_end_addr_ - 2 * page_size_;
  if (addr > last_page_addr)
    return ErrorType::kBufferOverflow;
  const uintptr_t offset = addr - first_page_addr_;
  DCHECK_NE((offset / page_size_) % 2, 0ULL);
  const size_t kHalfPageSize = page_size_ / 2;
  return (offset / kHalfPageSize) % 2 == 0 ? ErrorType::kBufferOverflow
                                           : ErrorType::kBufferUnderflow;
}

uintptr_t GuardedPageAllocator::SlotToAddr(size_t slot) const {
  DCHECK_LT(slot, kFreePagesNumBits);
  return first_page_addr_ + 2 * slot * page_size_;
}

size_t GuardedPageAllocator::AddrToSlot(uintptr_t addr) const {
  DCHECK_EQ(addr % page_size_, 0ULL);
  uintptr_t offset = addr - first_page_addr_;
  DCHECK_EQ((offset / page_size_) % 2, 0ULL);
  size_t slot = offset / page_size_ / 2;
  DCHECK_LT(slot, kFreePagesNumBits);
  return slot;
}

GuardedPageAllocator::SlotMetadata::SlotMetadata() {}

GuardedPageAllocator::SlotMetadata::~SlotMetadata() {
  if (!stacktrace_alloc)
    return;

  Reset();

  free(stacktrace_alloc);
  free(stacktrace_dealloc);
}

void GuardedPageAllocator::SlotMetadata::Init() {
  // new is not used so that we can explicitly call the constructor when we
  // want to collect a stack trace.
  stacktrace_alloc =
      static_cast<StackTrace*>(malloc(sizeof(*stacktrace_alloc)));
  CHECK(stacktrace_alloc);
  stacktrace_dealloc =
      static_cast<StackTrace*>(malloc(sizeof(*stacktrace_dealloc)));
  CHECK(stacktrace_dealloc);
}

void GuardedPageAllocator::SlotMetadata::Reset() {
  // Destruct previous allocation/deallocation traces. The constructor was only
  // called if (de)alloc_trace_addr is non-null.
  if (alloc_trace_addr)
    stacktrace_alloc->~StackTrace();
  if (dealloc_trace_addr)
    stacktrace_dealloc->~StackTrace();
}

void GuardedPageAllocator::SlotMetadata::RecordAllocation(size_t size,
                                                          size_t offset) {
  Reset();

  alloc_size = size;
  alloc_offset = offset;

  alloc_tid = base::PlatformThread::CurrentId();
  new (stacktrace_alloc) StackTrace();
  alloc_trace_addr = stacktrace_alloc->Addresses(&alloc_trace_len);

  dealloc_tid = base::kInvalidThreadId;
  dealloc_trace_addr = nullptr;
  dealloc_trace_len = 0;
}

void GuardedPageAllocator::SlotMetadata::RecordDeallocation() {
  dealloc_tid = base::PlatformThread::CurrentId();
  new (stacktrace_dealloc) StackTrace();
  dealloc_trace_addr = stacktrace_dealloc->Addresses(&dealloc_trace_len);
}

}  // namespace internal
}  // namespace gwp_asan
