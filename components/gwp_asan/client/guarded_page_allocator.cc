// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <algorithm>
#include <memory>

#include "base/bits.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/process/process_metrics.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "build/build_config.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/common/allocator_state.h"
#include "components/gwp_asan/common/crash_key_name.h"
#include "components/gwp_asan/common/pack_stack_trace.h"

#if defined(OS_MACOSX)
#include <pthread.h>
#endif

namespace gwp_asan {
namespace internal {

namespace {

// Report a tid that matches what crashpad collects which may differ from what
// base::PlatformThread::CurrentId() returns.
uint64_t ReportTid() {
#if !defined(OS_MACOSX)
  return base::PlatformThread::CurrentId();
#else
  uint64_t tid = base::kInvalidThreadId;
  pthread_threadid_np(nullptr, &tid);
  return tid;
#endif
}

}  // namespace

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
      free_slots_[i] = i;
    free_slots_end_ = total_pages;
  }

  metadata_ = std::make_unique<AllocatorState::SlotMetadata[]>(total_pages);
  state_.metadata_addr = reinterpret_cast<uintptr_t>(metadata_.get());
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

  AllocatorState::SlotIdx free_slot;
  if (!ReserveSlot(&free_slot))
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
  RecordAllocationMetadata(free_slot, size, alloc);

  return alloc;
}

void GuardedPageAllocator::Deallocate(void* ptr) {
  CHECK(PointerIsMine(ptr));

  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));

  // Check for a call to free() with an incorrect pointer (e.g. the pointer does
  // not match the allocated pointer.)
  if (addr != metadata_[slot].alloc_ptr) {
    state_.free_invalid_address = addr;
    __builtin_trap();
  }

  // Check for double free.
  if (metadata_[slot].deallocation_occurred.exchange(true)) {
    state_.double_free_address = addr;
    // TODO(https://crbug.com/925447): The other thread may not be done writing
    // a stack trace so we could spin here until it's read; however, it's also
    // possible we are racing an allocation in the middle of
    // RecordAllocationMetadata. For now it's possible a racy double free could
    // lead to a bad stack trace, but no internal allocator corruption.
    __builtin_trap();
  }

  // Record deallocation stack trace/thread id before marking the page
  // inaccessible in case a use-after-free occurs immediately.
  RecordDeallocationMetadata(slot);
  MarkPageInaccessible(reinterpret_cast<void*>(state_.GetPageAddr(addr)));

  FreeSlot(slot);
}

size_t GuardedPageAllocator::GetRequestedSize(const void* ptr) const {
  CHECK(PointerIsMine(ptr));
  const uintptr_t addr = reinterpret_cast<uintptr_t>(ptr);
  AllocatorState::SlotIdx slot = state_.AddrToSlot(state_.GetPageAddr(addr));
  DCHECK_EQ(addr, metadata_[slot].alloc_ptr);
  return metadata_[slot].alloc_size;
}

size_t GuardedPageAllocator::RegionSize() const {
  return (2 * state_.total_pages + 1) * state_.page_size;
}

bool GuardedPageAllocator::ReserveSlot(AllocatorState::SlotIdx* slot) {
  base::AutoLock lock(lock_);

  if (num_alloced_pages_ == max_alloced_pages_)
    return false;
  num_alloced_pages_++;

  DCHECK_GT(free_slots_end_, 0U);
  DCHECK_LE(free_slots_end_, state_.total_pages);
  size_t rand = base::RandGenerator(free_slots_end_);
  *slot = free_slots_[rand];
  free_slots_end_--;
  free_slots_[rand] = free_slots_[free_slots_end_];
  DCHECK_EQ(free_slots_end_, state_.total_pages - num_alloced_pages_);

  return true;
}

void GuardedPageAllocator::FreeSlot(AllocatorState::SlotIdx slot) {
  DCHECK_LT(slot, state_.total_pages);

  base::AutoLock lock(lock_);
  DCHECK_LT(free_slots_end_, state_.total_pages);
  free_slots_[free_slots_end_] = slot;
  free_slots_end_++;

  DCHECK_GT(num_alloced_pages_, 0U);
  num_alloced_pages_--;

  DCHECK_EQ(free_slots_end_, state_.total_pages - num_alloced_pages_);
}

void GuardedPageAllocator::RecordAllocationMetadata(
    AllocatorState::SlotIdx slot,
    size_t size,
    void* ptr) {
  metadata_[slot].alloc_size = size;
  metadata_[slot].alloc_ptr = reinterpret_cast<uintptr_t>(ptr);

  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[slot].alloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[slot].alloc.packed_trace,
           sizeof(metadata_[slot].alloc.packed_trace));
  metadata_[slot].alloc.tid = ReportTid();
  metadata_[slot].alloc.trace_collected = true;

  metadata_[slot].dealloc.tid = base::kInvalidThreadId;
  metadata_[slot].dealloc.trace_len = 0;
  metadata_[slot].dealloc.trace_collected = false;
  metadata_[slot].deallocation_occurred = false;
}

void GuardedPageAllocator::RecordDeallocationMetadata(
    AllocatorState::SlotIdx slot) {
  void* trace[AllocatorState::kMaxStackFrames];
  size_t len =
      base::debug::CollectStackTrace(trace, AllocatorState::kMaxStackFrames);
  metadata_[slot].dealloc.trace_len =
      Pack(reinterpret_cast<uintptr_t*>(trace), len,
           metadata_[slot].dealloc.packed_trace,
           sizeof(metadata_[slot].dealloc.packed_trace));
  metadata_[slot].dealloc.tid = ReportTid();
  metadata_[slot].dealloc.trace_collected = true;
}

uintptr_t GuardedPageAllocator::GetCrashKeyAddress() const {
  return reinterpret_cast<uintptr_t>(&state_);
}

}  // namespace internal
}  // namespace gwp_asan
