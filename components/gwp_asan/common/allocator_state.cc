// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/common/allocator_state.h"

#include "base/bits.h"
#include "base/process/process_metrics.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

using base::debug::StackTrace;

namespace gwp_asan {
namespace internal {

// TODO: Delete out-of-line constexpr defininitons once C++17 is in use.
constexpr size_t AllocatorState::kGpaMaxPages;

AllocatorState::GetMetadataReturnType AllocatorState::GetMetadataForAddress(
    uintptr_t exception_address,
    SlotMetadata* slot,
    ErrorType* error_type) const {
  CHECK(IsValid());

  if (!PointerIsMine(exception_address))
    return GetMetadataReturnType::kUnrelatedCrash;

  size_t slot_idx = GetNearestSlot(exception_address);
  if (slot_idx >= kGpaMaxPages)
    return GetMetadataReturnType::kErrorBadSlot;

  *slot = data[slot_idx];
  *error_type = GetErrorType(exception_address, slot->alloc.trace_addr != 0,
                             slot->dealloc.trace_addr != 0);

  return GetMetadataReturnType::kGwpAsanCrash;
}

bool AllocatorState::IsValid() const {
  if (!page_size || page_size != base::GetPageSize())
    return false;

  if (num_pages == 0 || num_pages > kGpaMaxPages)
    return false;

  if (pages_base_addr % page_size != 0 || pages_end_addr % page_size != 0 ||
      first_page_addr % page_size != 0)
    return false;

  if (pages_base_addr >= pages_end_addr)
    return false;

  if (first_page_addr != pages_base_addr + page_size ||
      pages_end_addr - pages_base_addr != page_size * (num_pages * 2 + 1))
    return false;

  return true;
}

uintptr_t AllocatorState::GetPageAddr(uintptr_t addr) const {
  const uintptr_t addr_mask = ~(page_size - 1ULL);
  return addr & addr_mask;
}

uintptr_t AllocatorState::GetNearestValidPage(uintptr_t addr) const {
  if (addr < first_page_addr)
    return first_page_addr;
  const uintptr_t last_page_addr = pages_end_addr - 2 * page_size;
  if (addr > last_page_addr)
    return last_page_addr;

  uintptr_t offset = addr - first_page_addr;
  // If addr is already on a valid page, just return addr.
  if ((offset >> base::bits::Log2Floor(page_size)) % 2 == 0)
    return addr;

  // ptr points to a guard page, so get nearest valid page.
  const size_t kHalfPageSize = page_size / 2;
  if ((offset >> base::bits::Log2Floor(kHalfPageSize)) % 2 == 0) {
    return addr - kHalfPageSize;  // Round down.
  }
  return addr + kHalfPageSize;  // Round up.
}

size_t AllocatorState::GetNearestSlot(uintptr_t addr) const {
  return AddrToSlot(GetPageAddr(GetNearestValidPage(addr)));
}

AllocatorState::ErrorType AllocatorState::GetErrorType(uintptr_t addr,
                                                       bool allocated,
                                                       bool deallocated) const {
  if (!allocated)
    return ErrorType::kUnknown;
  if (double_free_detected)
    return ErrorType::kDoubleFree;
  if (deallocated)
    return ErrorType::kUseAfterFree;
  if (addr < first_page_addr)
    return ErrorType::kBufferUnderflow;
  const uintptr_t last_page_addr = pages_end_addr - 2 * page_size;
  if (addr > last_page_addr)
    return ErrorType::kBufferOverflow;
  const uintptr_t offset = addr - first_page_addr;
  DCHECK_NE((offset >> base::bits::Log2Floor(page_size)) % 2, 0ULL);
  const size_t kHalfPageSize = page_size / 2;
  return (offset >> base::bits::Log2Floor(kHalfPageSize)) % 2 == 0
             ? ErrorType::kBufferOverflow
             : ErrorType::kBufferUnderflow;
}

uintptr_t AllocatorState::SlotToAddr(size_t slot) const {
  DCHECK_LT(slot, kGpaMaxPages);
  return first_page_addr + 2 * slot * page_size;
}

size_t AllocatorState::AddrToSlot(uintptr_t addr) const {
  DCHECK_EQ(addr % page_size, 0ULL);
  uintptr_t offset = addr - first_page_addr;
  DCHECK_EQ((offset >> base::bits::Log2Floor(page_size)) % 2, 0ULL);
  size_t slot = (offset >> base::bits::Log2Floor(page_size)) / 2;
  DCHECK_LT(slot, kGpaMaxPages);
  return slot;
}

}  // namespace internal
}  // namespace gwp_asan
