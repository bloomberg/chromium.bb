// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "components/gwp_asan/common/guarded_page_allocator.h"

#include "base/bits.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace gwp_asan {
namespace internal {

unsigned CountTrailingZeroBits64(uint64_t x) {
#if defined(ARCH_CPU_64_BITS)
  return base::bits::CountTrailingZeroBits(x);
#else
  // Windows 32-bit builds do not support CountTrailingZeroBits on a uint64_t.
  // TODO(vtsyrklevich): Fix this in base::bits instead.
  uint32_t right = static_cast<uint32_t>(x);
  unsigned right_trailing = base::bits::CountTrailingZeroBits(right);
  if (right_trailing < 32)
    return right_trailing;

  uint32_t left = static_cast<uint32_t>(x >> 32);
  unsigned left_trailing = base::bits::CountTrailingZeroBits(left);
  return left_trailing + right_trailing;
#endif
}

// TODO(vtsyrklevich): See if the platform-specific memory allocation and
// protection routines can be broken out in base/ and merged with those used for
// PartionAlloc/ProtectedMemory.
bool GuardedPageAllocator::MapPages() {
  size_t len = (2 * num_pages_ + 1) * page_size_;
  void* base_ptr = VirtualAlloc(nullptr, len, MEM_RESERVE, PAGE_NOACCESS);
  if (!base_ptr) {
    DPLOG(ERROR) << "Failed to reserve guarded allocator region";
    return false;
  }

  uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_ptr);

  // Commit the pages used for allocations.
  for (size_t i = 0; i < num_pages_; i++) {
    uintptr_t address = base_addr + page_size_ * ((2 * i) + 1);
    LPVOID ret = VirtualAlloc(reinterpret_cast<LPVOID>(address), page_size_,
                              MEM_COMMIT, PAGE_NOACCESS);
    if (!ret) {
      PLOG(ERROR) << "Failed to commit allocation page";
      UnmapPages();
      return false;
    }
  }

  pages_base_addr_ = base_addr;
  first_page_addr_ = pages_base_addr_ + page_size_;
  pages_end_addr_ = pages_base_addr_ + len;

  return true;
}

void GuardedPageAllocator::UnmapPages() {
  DCHECK(pages_base_addr_);
  BOOL err =
      VirtualFree(reinterpret_cast<void*>(pages_base_addr_), 0, MEM_RELEASE);
  DCHECK(err);
  (void)err;
}

void GuardedPageAllocator::MarkPageReadWrite(void* ptr) {
  DWORD old_prot;
  BOOL err = VirtualProtect(ptr, page_size_, PAGE_READWRITE, &old_prot);
  PCHECK(err != 0) << "VirtualProtect";
}

void GuardedPageAllocator::MarkPageInaccessible(void* ptr) {
  DWORD old_prot;
  BOOL err = VirtualProtect(ptr, page_size_, PAGE_NOACCESS, &old_prot);
  PCHECK(err != 0) << "VirtualProtect";
}

}  // namespace internal
}  // namespace gwp_asan
