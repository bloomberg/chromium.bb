// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include "base/logging.h"
#include "build/build_config.h"

namespace gwp_asan {
namespace internal {

// TODO(vtsyrklevich): See if the platform-specific memory allocation and
// protection routines can be broken out in base/ and merged with those used for
// PartionAlloc/ProtectedMemory.
bool GuardedPageAllocator::MapPages() {
  size_t len = (2 * state_.num_pages + 1) * state_.page_size;
  void* base_ptr = VirtualAlloc(nullptr, len, MEM_RESERVE, PAGE_NOACCESS);
  if (!base_ptr) {
    DPLOG(ERROR) << "Failed to reserve guarded allocator region";
    return false;
  }

  uintptr_t base_addr = reinterpret_cast<uintptr_t>(base_ptr);

  // Commit the pages used for allocations.
  for (size_t i = 0; i < state_.num_pages; i++) {
    uintptr_t address = base_addr + state_.page_size * ((2 * i) + 1);
    LPVOID ret = VirtualAlloc(reinterpret_cast<LPVOID>(address),
                              state_.page_size, MEM_COMMIT, PAGE_NOACCESS);
    if (!ret) {
      PLOG(ERROR) << "Failed to commit allocation page";
      UnmapPages();
      return false;
    }
  }

  state_.pages_base_addr = base_addr;
  state_.first_page_addr = state_.pages_base_addr + state_.page_size;
  state_.pages_end_addr = state_.pages_base_addr + len;

  return true;
}

void GuardedPageAllocator::UnmapPages() {
  DCHECK(state_.pages_base_addr);
  BOOL err = VirtualFree(reinterpret_cast<void*>(state_.pages_base_addr), 0,
                         MEM_RELEASE);
  DCHECK(err);
  (void)err;
}

void GuardedPageAllocator::MarkPageReadWrite(void* ptr) {
  DWORD old_prot;
  BOOL err = VirtualProtect(ptr, state_.page_size, PAGE_READWRITE, &old_prot);
  PCHECK(err != 0) << "VirtualProtect";
}

void GuardedPageAllocator::MarkPageInaccessible(void* ptr) {
  DWORD old_prot;
  BOOL err = VirtualProtect(ptr, state_.page_size, PAGE_NOACCESS, &old_prot);
  PCHECK(err != 0) << "VirtualProtect";
}

}  // namespace internal
}  // namespace gwp_asan
