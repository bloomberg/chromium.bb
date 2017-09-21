// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/freed_object_tracker.h"

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "build/build_config.h"

namespace base {
namespace trace_event {

namespace {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)

using base::allocator::AllocatorDispatch;

void* HookAlloc(const AllocatorDispatch* self, size_t size, void* context) {
  const AllocatorDispatch* const next = self->next;
  return next->alloc_function(next, size, context);
}

void* HookZeroInitAlloc(const AllocatorDispatch* self,
                        size_t n,
                        size_t size,
                        void* context) {
  const AllocatorDispatch* const next = self->next;
  return next->alloc_zero_initialized_function(next, n, size, context);
}

void* HookAllocAligned(const AllocatorDispatch* self,
                       size_t alignment,
                       size_t size,
                       void* context) {
  const AllocatorDispatch* const next = self->next;
  return next->alloc_aligned_function(next, alignment, size, context);
}

void* HookRealloc(const AllocatorDispatch* self,
                  void* address,
                  size_t size,
                  void* context) {
  const AllocatorDispatch* const next = self->next;
  void* new_addr = next->realloc_function(next, address, size, context);
  if (size == 0 || new_addr != address)
    FreedObjectTracker::GetInstance()->RemoveAllocation(address);
  return new_addr;
}

void HookFree(const AllocatorDispatch* self, void* address, void* context) {
  if (address)
    FreedObjectTracker::GetInstance()->RemoveAllocation(address);
  const AllocatorDispatch* const next = self->next;
  next->free_function(next, address, context);
}

size_t HookGetSizeEstimate(const AllocatorDispatch* self,
                           void* address,
                           void* context) {
  const AllocatorDispatch* const next = self->next;
  return next->get_size_estimate_function(next, address, context);
}

unsigned HookBatchMalloc(const AllocatorDispatch* self,
                         size_t size,
                         void** results,
                         unsigned num_requested,
                         void* context) {
  const AllocatorDispatch* const next = self->next;
  return next->batch_malloc_function(next, size, results, num_requested,
                                     context);
}

void HookBatchFree(const AllocatorDispatch* self,
                   void** to_be_freed,
                   unsigned num_to_be_freed,
                   void* context) {
  for (unsigned i = 0; i < num_to_be_freed; ++i) {
    FreedObjectTracker::GetInstance()->RemoveAllocation(to_be_freed[i]);
  }
  const AllocatorDispatch* const next = self->next;
  next->batch_free_function(next, to_be_freed, num_to_be_freed, context);
}

void HookFreeDefiniteSize(const AllocatorDispatch* self,
                          void* address,
                          size_t size,
                          void* context) {
  if (address)
    FreedObjectTracker::GetInstance()->RemoveAllocation(address);
  const AllocatorDispatch* const next = self->next;
  next->free_definite_size_function(next, address, size, context);
}

AllocatorDispatch g_allocator_hooks = {
    &HookAlloc,            /* alloc_function */
    &HookZeroInitAlloc,    /* alloc_zero_initialized_function */
    &HookAllocAligned,     /* alloc_aligned_function */
    &HookRealloc,          /* realloc_function */
    &HookFree,             /* free_function */
    &HookGetSizeEstimate,  /* get_size_estimate_function */
    &HookBatchMalloc,      /* batch_malloc_function */
    &HookBatchFree,        /* batch_free_function */
    &HookFreeDefiniteSize, /* free_definite_size_function */
    nullptr,               /* next */
};

#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}  // namespace

LazyInstance<FreedObjectTracker>::Leaky g_freed_object_tracker;

// static
FreedObjectTracker* FreedObjectTracker::GetInstance() {
  return g_freed_object_tracker.Pointer();
}

FreedObjectTracker::FreedObjectTracker() {}

FreedObjectTracker::~FreedObjectTracker() {}

void FreedObjectTracker::Enable() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocation_register_.SetEnabled();
  allocator::InsertAllocatorDispatch(&g_allocator_hooks);

#if !DCHECK_IS_ON()
  // This causes a NOTREACHED exception in OnThreadExitInternal trying to call
  // a destructor but it works fine when DCHECK isn't enabled.
  base::trace_event::AllocationContextTracker::SetCaptureMode(
      base::trace_event::AllocationContextTracker::CaptureMode::NATIVE_STACK);
#endif
#endif
}

void FreedObjectTracker::DisableForTesting() {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  allocation_register_.SetDisabled();
  allocator::RemoveAllocatorDispatchForTesting(&g_allocator_hooks);
#endif
}

bool FreedObjectTracker::Get(
    const void* address,
    AllocationRegister::Allocation* out_allocation) const {
  if (!allocation_register_.is_enabled())
    return false;

  return allocation_register_.Get(address, out_allocation);
}

void FreedObjectTracker::RemoveAllocation(void* address) {
  if (!allocation_register_.is_enabled())
    return;

  auto* tracker = AllocationContextTracker::GetInstanceForCurrentThread();
  if (!tracker)
    return;

  AllocationContext context;
  if (!tracker->GetContextSnapshot(&context))
    return;

  // Original size is unknown. 0 won't be recorded.
  allocation_register_.Insert(address, 1, context);
}

}  // namespace trace_event
}  // namespace base
