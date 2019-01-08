// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/guarded_page_allocator.h"

#include <sys/mman.h>

#include "base/logging.h"

namespace gwp_asan {
namespace internal {

void* GuardedPageAllocator::MapRegion() {
  return mmap(nullptr, RegionSize(), PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1,
              0);
}

void GuardedPageAllocator::UnmapRegion() {
  CHECK(state_.pages_base_addr);
  int err =
      munmap(reinterpret_cast<void*>(state_.pages_base_addr), RegionSize());
  DCHECK_EQ(err, 0);
  (void)err;
}

void GuardedPageAllocator::MarkPageReadWrite(void* ptr) {
  int err = mprotect(ptr, state_.page_size, PROT_READ | PROT_WRITE);
  PCHECK(err == 0) << "mprotect";
}

void GuardedPageAllocator::MarkPageInaccessible(void* ptr) {
  int err = mprotect(ptr, state_.page_size, PROT_NONE);
  PCHECK(err == 0) << "mprotect";
}

}  // namespace internal
}  // namespace gwp_asan
