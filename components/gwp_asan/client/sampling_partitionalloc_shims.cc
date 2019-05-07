// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/sampling_partitionalloc_shims.h"

#include <algorithm>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/logging.h"
#include "base/partition_alloc_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "components/gwp_asan/client/export.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_state.h"
#include "components/gwp_asan/common/crash_key_name.h"

namespace gwp_asan {
namespace internal {

namespace {

SamplingState<PARTITIONALLOC> sampling_state;

// The global allocator singleton used by the shims. Implemented as a global
// pointer instead of a function-local static to avoid initialization checks
// for every access.
GuardedPageAllocator* gpa = nullptr;

// TODO(vtsyrklevich): PartitionAlloc ensures that different typed allocations
// never overlap. For now, we ensure this property by only allowing one
// allocation for every page. In the future we need to teach the allocator about
// types so that it keeps track and pages can be reused.
size_t allocation_counter = 0;
size_t total_allocations = 0;

bool AllocationHook(void** out, int flags, size_t size, const char* type_name) {
  if (UNLIKELY(sampling_state.Sample())) {
    // Ignore allocation requests with unknown flags.
    constexpr int kKnownFlags =
        base::PartitionAllocReturnNull | base::PartitionAllocZeroFill;
    if (flags & ~kKnownFlags)
      return false;

    // Ensure PartitionAlloc types are separated for now.
    if (allocation_counter >= total_allocations)
      return false;
    allocation_counter++;

    if (void* allocation = gpa->Allocate(size)) {
      *out = allocation;
      return true;
    }
  }
  return false;
}

bool FreeHook(void* address) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    gpa->Deallocate(address);
    return true;
  }
  return false;
}

bool ReallocHook(size_t* out, void* address) {
  if (UNLIKELY(gpa->PointerIsMine(address))) {
    *out = gpa->GetRequestedSize(address);
    return true;
  }
  return false;
}

}  // namespace

// We expose the allocator singleton for unit tests.
GWP_ASAN_EXPORT GuardedPageAllocator& GetPartitionAllocGpaForTesting() {
  return *gpa;
}

void InstallPartitionAllocHooks(size_t max_allocated_pages,
                                size_t num_metadata,
                                size_t total_pages,
                                size_t sampling_frequency) {
  static crash_reporter::CrashKeyString<24> pa_crash_key(
      kPartitionAllocCrashKey);
  gpa = new GuardedPageAllocator();
  gpa->Init(max_allocated_pages, num_metadata, total_pages);
  pa_crash_key.Set(gpa->GetCrashKey());
  sampling_state.Init(sampling_frequency);
  total_allocations = total_pages;
  // TODO(vtsyrklevich): Allow SetOverrideHooks to be passed in so we can hook
  // PDFium's PartitionAlloc fork.
  base::PartitionAllocHooks::SetOverrideHooks(&AllocationHook, &FreeHook,
                                              &ReallocHook);
}

}  // namespace internal
}  // namespace gwp_asan
