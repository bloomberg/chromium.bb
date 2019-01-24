// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <iterator>
#include <memory>

#include "base/atomicops.h"
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

  slots_ = std::make_unique<AllocatorState::SlotMetadata[]>(total_pages);
  state_.slot_metadata = reinterpret_cast<uintptr_t>(slots_.get());
}

GuardedPageAllocator::~GuardedPageAllocator() {
  if (state_.total_pages)
    UnmapRegion();
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
  if (free_slot & 1)
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
  DCHECK_EQ(addr, slots_[slot].alloc_ptr);
  // Check for double free.
  if (slots_[slot].dealloc.trace_collected) {
    state_.double_free_detected = true;
    base::subtle::MemoryBarrier();
    // Trigger an exception by writing to the inaccessible free()d allocation.
    // We want to crash by accessing the offending allocation in order to signal
    // to the crash handler that this crash is caused by that allocation.
    *reinterpret_cast<char*>(ptr) = 'X';
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
  DCHECK_EQ(addr, slots_[slot].alloc_ptr);
  return slots_[slot].alloc_size;
}

size_t GuardedPageAllocator::RegionSize() const {
  return (2 * state_.total_pages + 1) * state_.page_size;
}

size_t GuardedPageAllocator::ReserveSlot() {
  base::AutoLock lock(lock_);

  if (num_alloced_pages_ == max_alloced_pages_)
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

void GuardedPageAllocator::RecordAllocationInSlot(size_t slot,
                                                  size_t size,
                                                  void* ptr) {
  slots_[slot].alloc_size = size;
  slots_[slot].alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  slots_[slot].alloc.tid = base::PlatformThread::CurrentId();
  slots_[slot].alloc.trace_len = base::debug::CollectStackTrace(
      reinterpret_cast<void**>(&slots_[slot].alloc.trace),
      AllocatorState::kMaxStackFrames);
  slots_[slot].alloc.trace_collected = true;

  slots_[slot].dealloc.tid = base::kInvalidThreadId;
  slots_[slot].dealloc.trace_len = 0;
  slots_[slot].dealloc.trace_collected = false;
}

void GuardedPageAllocator::RecordDeallocationInSlot(size_t slot) {
  slots_[slot].dealloc.tid = base::PlatformThread::CurrentId();
  slots_[slot].dealloc.trace_len = base::debug::CollectStackTrace(
      reinterpret_cast<void**>(&slots_[slot].dealloc.trace),
      AllocatorState::kMaxStackFrames);
  slots_[slot].dealloc.trace_collected = true;
}

uintptr_t GuardedPageAllocator::GetCrashKeyAddress() const {
  return reinterpret_cast<uintptr_t>(&state_);
}

}  // namespace internal
}  // namespace gwp_asan
