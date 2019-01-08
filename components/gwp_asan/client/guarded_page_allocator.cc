// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <iterator>

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

void GuardedPageAllocator::Init(size_t max_alloced_pages, size_t total_pages) {
  CHECK_GT(max_alloced_pages, 0U);
  CHECK_LE(max_alloced_pages, total_pages);
  CHECK_LE(total_pages, AllocatorState::kGpaMaxPages);
  max_alloced_pages_ = max_alloced_pages;
  state_.total_pages = total_pages;

  state_.page_size = base::GetPageSize();

  void* region = MapRegion();
  if (!region)
    PLOG(FATAL) << "Failed to reserve allocator region";

  state_.pages_base_addr = reinterpret_cast<uintptr_t>(region);
  state_.first_page_addr = state_.pages_base_addr + state_.page_size;
  state_.pages_end_addr = state_.pages_base_addr + RegionSize();

  {
    // Obtain this lock exclusively to satisfy the thread-safety annotations,
    // there should be no risk of a race here.
    base::AutoLock lock(lock_);
    for (size_t i = 0; i < total_pages; i++)
      free_slot_ring_buffer_[i] = static_cast<SlotTy>(i);
    base::RandomShuffle(free_slot_ring_buffer_.begin(),
                        std::next(free_slot_ring_buffer_.begin(), total_pages));
  }

  AllocateStackTraces();
}

GuardedPageAllocator::~GuardedPageAllocator() {
  if (state_.total_pages) {
    UnmapRegion();
    DeallocateStackTraces();
  }
}

void* GuardedPageAllocator::Allocate(size_t size, size_t align) {
  if (!size || size > state_.page_size || align > state_.page_size)
    return nullptr;

  // Default alignment is size's next smallest power-of-two, up to
  // kGpaAllocAlignment.
  if (!align) {
    align =
        std::min(size_t{1} << base::bits::Log2Floor(size), kGpaAllocAlignment);
  }
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
  size_t slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  DCHECK_EQ(addr, state_.data[slot].alloc_ptr);
  // Check for double free.
  if (state_.data[slot].dealloc.trace_addr) {
    state_.double_free_detected = true;
    *reinterpret_cast<char*>(ptr) = 'X';  // Trigger exception.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id before marking the page
  // inaccessible in case a use-after-free occurs immediately.
  RecordDeallocationInSlot(slot);
  MarkPageInaccessible(reinterpret_cast<void*>(state_.GetPageAddr(addr)));

  FreeSlot(slot);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  CHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  size_t slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  DCHECK_EQ(addr, state_.data[slot].alloc_ptr);
  return state_.data[slot].alloc_size;
}

size_t GuardedPageAllocator::RegionSize() const {
  return (2 * state_.total_pages + 1) * state_.page_size;
}

size_t GuardedPageAllocator::ReserveSlot() {
  base::AutoLock lock(lock_);

  if (num_alloced_pages_ == max_alloced_pages_)
    return SIZE_MAX;

  // Disable allocations after a double free is detected so that the double
  // freed allocation is not reallocated while the crash handler could be
  // concurrently inspecting the metadata.
  if (state_.double_free_detected)
    return SIZE_MAX;

  SlotTy slot = free_slot_ring_buffer_[free_slot_start_idx_];
  free_slot_start_idx_ = (free_slot_start_idx_ + 1) % state_.total_pages;
  DCHECK_LT(slot, state_.total_pages);
  num_alloced_pages_++;
  DCHECK_EQ((free_slot_end_idx_ + num_alloced_pages_) % state_.total_pages,
            free_slot_start_idx_);
  return slot;
}

void GuardedPageAllocator::FreeSlot(size_t slot) {
  DCHECK_LT(slot, state_.total_pages);

  base::AutoLock lock(lock_);
  DCHECK_GT(num_alloced_pages_, 0U);
  num_alloced_pages_--;
  free_slot_ring_buffer_[free_slot_end_idx_] = static_cast<SlotTy>(slot);
  free_slot_end_idx_ = (free_slot_end_idx_ + 1) % state_.total_pages;
  DCHECK_EQ((free_slot_end_idx_ + num_alloced_pages_) % state_.total_pages,
            free_slot_start_idx_);
}

void GuardedPageAllocator::AllocateStackTraces() {
  // new is not used so that we can explicitly call the constructor when we
  // want to collect a stack trace.
  for (size_t i = 0; i < state_.total_pages; i++) {
    alloc_traces[i] =
        static_cast<StackTrace*>(malloc(sizeof(*alloc_traces[i])));
    CHECK(alloc_traces[i]);
    dealloc_traces[i] =
        static_cast<StackTrace*>(malloc(sizeof(*dealloc_traces[i])));
    CHECK(dealloc_traces[i]);
  }
}

void GuardedPageAllocator::DeallocateStackTraces() {
  for (size_t i = 0; i < state_.total_pages; i++) {
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
