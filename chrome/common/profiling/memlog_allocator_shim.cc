// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_allocator_shim.h"

#include "base/allocator/allocator_shim.h"
#include "base/debug/debugging_flags.h"
#include "base/debug/stack_trace.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "chrome/common/profiling/memlog_stream.h"

namespace profiling {

namespace {

using base::allocator::AllocatorDispatch;

MemlogSenderPipe* g_sender_pipe = nullptr;

// This maximum number of stack entries to log. Long-term we will likely want
// to raise this to avoid truncation. This matches the current value in the
// in-process heap profiler (heap_profiler_allocation_context.h) so the two
// systems performance and memory overhead can be compared consistently.
const int kMaxStackEntries = 48;

// Matches the native buffer size on the pipe.
const int kSendBufferSize = 65536;

// Prime since this is used like a hash table. Numbers of this magnitude seemed
// to provide sufficient parallelism to avoid lock overhead in ad-hoc testing.
const int kNumSendBuffers = 17;

class SendBuffer {
 public:
  SendBuffer() : buffer_(new char[kSendBufferSize]) {}
  ~SendBuffer() { delete[] buffer_; }

  void Send(const void* data, size_t sz) {
    base::AutoLock lock(lock_);

    if (used_ + sz > kSendBufferSize)
      SendCurrentBuffer();

    memcpy(&buffer_[used_], data, sz);
    used_ += sz;
  }

 private:
  void SendCurrentBuffer() {
    g_sender_pipe->Send(buffer_, used_);
    used_ = 0;
  }

  base::Lock lock_;

  char* buffer_;
  size_t used_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SendBuffer);
};

SendBuffer* g_send_buffers = nullptr;

// "address" is the address in question, which is used to select which send
// buffer to use.
void DoSend(const void* address, const void* data, size_t size) {
  base::trace_event::AllocationRegister::AddressHasher hasher;
  int bin_to_use = hasher(address) % kNumSendBuffers;
  g_send_buffers[bin_to_use].Send(data, size);
}

void* HookAlloc(const AllocatorDispatch* self, size_t size, void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_function(next, size, context);
  AllocatorShimLogAlloc(ptr, size);
  return ptr;
}

void* HookZeroInitAlloc(const AllocatorDispatch* self,
                        size_t n,
                        size_t size,
                        void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_zero_initialized_function(next, n, size, context);
  AllocatorShimLogAlloc(ptr, n * size);
  return ptr;
}

void* HookAllocAligned(const AllocatorDispatch* self,
                       size_t alignment,
                       size_t size,
                       void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_aligned_function(next, alignment, size, context);
  AllocatorShimLogAlloc(ptr, size);
  return ptr;
}

void* HookRealloc(const AllocatorDispatch* self,
                  void* address,
                  size_t size,
                  void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->realloc_function(next, address, size, context);
  AllocatorShimLogFree(address);
  if (size > 0)  // realloc(size == 0) means free()
    AllocatorShimLogAlloc(ptr, size);
  return ptr;
}

void HookFree(const AllocatorDispatch* self, void* address, void* context) {
  AllocatorShimLogFree(address);
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
  unsigned count =
      next->batch_malloc_function(next, size, results, num_requested, context);
  for (unsigned i = 0; i < count; ++i)
    AllocatorShimLogAlloc(results[i], size);
  return count;
}

void HookBatchFree(const AllocatorDispatch* self,
                   void** to_be_freed,
                   unsigned num_to_be_freed,
                   void* context) {
  const AllocatorDispatch* const next = self->next;
  for (unsigned i = 0; i < num_to_be_freed; ++i)
    AllocatorShimLogFree(to_be_freed[i]);
  next->batch_free_function(next, to_be_freed, num_to_be_freed, context);
}

void HookFreeDefiniteSize(const AllocatorDispatch* self,
                          void* ptr,
                          size_t size,
                          void* context) {
  AllocatorShimLogFree(ptr);
  const AllocatorDispatch* const next = self->next;
  next->free_definite_size_function(next, ptr, size, context);
}

AllocatorDispatch g_memlog_hooks = {
    &HookAlloc,             // alloc_function
    &HookZeroInitAlloc,     // alloc_zero_initialized_function
    &HookAllocAligned,      // alloc_aligned_function
    &HookRealloc,           // realloc_function
    &HookFree,              // free_function
    &HookGetSizeEstimate,   // get_size_estimate_function
    &HookBatchMalloc,       // batch_malloc_function
    &HookBatchFree,         // batch_free_function
    &HookFreeDefiniteSize,  // free_definite_size_function
    nullptr,                // next
};

}  // namespace

void InitAllocatorShim(MemlogSenderPipe* sender_pipe) {
  g_send_buffers = new SendBuffer[kNumSendBuffers];

  g_sender_pipe = sender_pipe;
#ifdef NDEBUG
  base::allocator::InsertAllocatorDispatch(&g_memlog_hooks);
#endif
}

void AllocatorShimLogAlloc(void* address, size_t sz) {
  if (!g_send_buffers)
    return;
  if (address) {
    constexpr size_t max_message_size =
        sizeof(AllocPacket) + kMaxStackEntries * sizeof(uint64_t);
    char message[max_message_size];
    // TODO(ajwong) check that this is technically valid.
    AllocPacket* alloc_packet = reinterpret_cast<AllocPacket*>(message);

    uint64_t* stack =
        reinterpret_cast<uint64_t*>(&message[sizeof(AllocPacket)]);

#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
    const void* frames[kMaxStackEntries];
    size_t frame_count = base::debug::TraceStackFramePointers(
        frames, kMaxStackEntries,
        1);  // exclude this function from the trace.
#else   // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
    // Fall-back to capturing the stack with base::debug::StackTrace,
    // which is likely slower, but more reliable.
    base::debug::StackTrace stack_trace(kMaxStackEntries);
    size_t frame_count = 0u;
    const void* const* frames = stack_trace.Addresses(&frame_count);
#endif  // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

    // If there are too many frames, keep the ones furthest from main().
    for (size_t i = 0; i < frame_count; i++)
      stack[i] = (uint64_t)frames[i];

    alloc_packet->op = kAllocPacketType;
    alloc_packet->time = 0;  // TODO(brettw) add timestamp.
    alloc_packet->address = (uint64_t)address;
    alloc_packet->size = sz;
    alloc_packet->stack_len = static_cast<uint32_t>(frame_count);

    DoSend(address, message,
           sizeof(AllocPacket) + alloc_packet->stack_len * sizeof(uint64_t));
  }
}

void AllocatorShimLogFree(void* address) {
  if (!g_send_buffers)
    return;
  if (address) {
    FreePacket free_packet;
    free_packet.op = kFreePacketType;
    free_packet.time = 0;  // TODO(brettw) add timestamp.
    free_packet.address = (uint64_t)address;

    DoSend(address, &free_packet, sizeof(FreePacket));
  }
}

}  // namespace profiling
