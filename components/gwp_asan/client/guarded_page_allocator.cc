// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include "base/bits.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"

using base::debug::StackTrace;

namespace gwp_asan {
namespace internal {

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t GuardedPageAllocator::kGpaAllocAlignment;

GuardedPageAllocator::GuardedPageAllocator() {}

void GuardedPageAllocator::Init(size_t num_pages) {
  CHECK_GT(num_pages, 0U);
  CHECK_LE(num_pages, AllocatorState::kGpaMaxPages);
  state_.num_pages = num_pages;

  state_.page_size = base::GetPageSize();
  CHECK(MapPages());

  {
    // Obtain this lock exclusively to satisfy the thread-safety annotations,
    // there should be no risk of a race here.
    base::AutoLock lock(lock_);
    free_pages_ = (state_.num_pages == AllocatorState::kGpaMaxPages)
                      ? ~0ULL
                      : (1ULL << state_.num_pages) - 1;
  }

  AllocateStackTraces();
}

GuardedPageAllocator::~GuardedPageAllocator() {
  if (state_.num_pages) {
    UnmapPages();
    DeallocateStackTraces();
  }
}

void* GuardedPageAllocator::Allocate(size_t size, size_t align) {
  if (!size || size > state_.page_size)
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

  uintptr_t free_page = state_.SlotToAddr(free_slot);
  MarkPageReadWrite(reinterpret_cast<void*>(free_page));

  size_t offset;
  if (base::RandInt(0, 1))
    // Return right-aligned allocation to detect overflows.
    offset = state_.page_size - base::bits::Align(size, align);
  else
    // Return left-aligned allocation to detect underflows.
    offset = 0;

  void* alloc = reinterpret_cast<void*>(free_page + offset);

  // Initialize slot metadata.
  RecordAllocationInSlot(free_slot, size, alloc);

  return alloc;
}

void GuardedPageAllocator::Deallocate(void* ptr) {
  CHECK(PointerIsMine(ptr));

  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  MarkPageInaccessible(reinterpret_cast<void*>(state_.GetPageAddr(addr)));

  size_t slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  DCHECK_EQ(addr, state_.data[slot].alloc_ptr);
  // Check for double free.
  if (state_.data[slot].dealloc.trace_addr) {
    state_.double_free_detected = true;
    *reinterpret_cast<char*>(ptr) = 'X';  // Trigger exception.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id.
  RecordDeallocationInSlot(slot);

  FreeSlot(slot);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  CHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  size_t slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  DCHECK_EQ(addr, state_.data[slot].alloc_ptr);
  return state_.data[slot].alloc_size;
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
  if (state_.double_free_detected)
    return SIZE_MAX;

  uint64_t rot = base::RandGenerator(AllocatorState::kGpaMaxPages);
  BitMap rotated_bitmap = (free_pages_ << rot) |
                          (free_pages_ >> (AllocatorState::kGpaMaxPages - rot));
  int rotated_selection = base::bits::CountTrailingZeroBits(rotated_bitmap);
  size_t selection = (rotated_selection - rot + AllocatorState::kGpaMaxPages) %
                     AllocatorState::kGpaMaxPages;
  DCHECK_LT(selection, AllocatorState::kGpaMaxPages);
  DCHECK(free_pages_ & (1ULL << selection));
  free_pages_ &= ~(1ULL << selection);
  return selection;
}

void GuardedPageAllocator::FreeSlot(size_t slot) {
  DCHECK_LT(slot, AllocatorState::kGpaMaxPages);

  BitMap bit = 1ULL << slot;
  base::AutoLock lock(lock_);
  DCHECK_EQ((free_pages_ & bit), 0ULL);
  free_pages_ |= bit;
}

void GuardedPageAllocator::AllocateStackTraces() {
  // new is not used so that we can explicitly call the constructor when we
  // want to collect a stack trace.
  for (size_t i = 0; i < state_.num_pages; i++) {
    alloc_traces[i] =
        static_cast<StackTrace*>(malloc(sizeof(*alloc_traces[i])));
    CHECK(alloc_traces[i]);
    dealloc_traces[i] =
        static_cast<StackTrace*>(malloc(sizeof(*dealloc_traces[i])));
    CHECK(dealloc_traces[i]);
  }
}

void GuardedPageAllocator::DeallocateStackTraces() {
  for (size_t i = 0; i < state_.num_pages; i++) {
    DestructStackTrace(i);

    free(alloc_traces[i]);
    alloc_traces[i] = nullptr;
    free(dealloc_traces[i]);
    dealloc_traces[i] = nullptr;
  }
}

void GuardedPageAllocator::DestructStackTrace(size_t slot) {
  // Destruct previous allocation/deallocation traces. The constructor was only
  // called if trace_addr is non-null.
  if (state_.data[slot].alloc.trace_addr)
    alloc_traces[slot]->~StackTrace();
  if (state_.data[slot].dealloc.trace_addr)
    dealloc_traces[slot]->~StackTrace();
}

void GuardedPageAllocator::RecordAllocationInSlot(size_t slot,
                                                  size_t size,
                                                  void* ptr) {
  state_.data[slot].alloc_size = size;
  state_.data[slot].alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  state_.data[slot].alloc.tid = base::PlatformThread::CurrentId();
  new (alloc_traces[slot]) StackTrace();
  state_.data[slot].alloc.trace_addr = reinterpret_cast<uintptr_t>(
      alloc_traces[slot]->Addresses(&state_.data[slot].alloc.trace_len));

  state_.data[slot].dealloc.tid = base::kInvalidThreadId;
  state_.data[slot].dealloc.trace_addr = 0;
  state_.data[slot].dealloc.trace_len = 0;
}

void GuardedPageAllocator::RecordDeallocationInSlot(size_t slot) {
  state_.data[slot].dealloc.tid = base::PlatformThread::CurrentId();
  new (dealloc_traces[slot]) StackTrace();
  state_.data[slot].dealloc.trace_addr = reinterpret_cast<uintptr_t>(
      dealloc_traces[slot]->Addresses(&state_.data[slot].dealloc.trace_len));
}

uintptr_t GuardedPageAllocator::GetCrashKeyAddress() const {
  return reinterpret_cast<uintptr_t>(&state_);
}

}  // namespace internal
}  // namespace gwp_asan
