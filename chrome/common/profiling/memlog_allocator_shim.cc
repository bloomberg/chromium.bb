// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_allocator_shim.h"

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/compiler_specific.h"
#include "base/debug/debugging_flags.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "build/build_config.h"
#include "chrome/common/profiling/memlog_stream.h"

#if defined(OS_POSIX)
#include <limits.h>
#endif

namespace profiling {

namespace {

using base::allocator::AllocatorDispatch;

MemlogSenderPipe* g_sender_pipe = nullptr;

// Matches the native buffer size on the pipe.
// On Windows and Linux, the default pipe buffer size is 65536.
// On macOS, the default pipe buffer size is 16 * 1024, but grows to 64 * 1024
// for large writes.
constexpr int kSendBufferSize = 65536;

// Prime since this is used like a hash table. Numbers of this magnitude seemed
// to provide sufficient parallelism to avoid lock overhead in ad-hoc testing.
constexpr int kNumSendBuffers = 17;

// Functions set by a callback if the GC heap exists in the current process.
// This function pointers can be used to hook or unhook the oilpan allocations.
// It will be null in the browser process.
SetGCAllocHookFunction g_hook_gc_alloc = nullptr;
SetGCFreeHookFunction g_hook_gc_free = nullptr;

// In the very unlikely scenario where a thread has grabbed the SendBuffer lock,
// and then performs a heap allocation/free, ignore the allocation. Failing to
// do so will cause non-deterministic deadlock, depending on whether the
// allocation is dispatched to the same SendBuffer.
base::LazyInstance<base::ThreadLocalBoolean>::Leaky g_prevent_reentrancy =
    LAZY_INSTANCE_INITIALIZER;

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

  void Flush() {
    base::AutoLock lock(lock_);
    if (used_ > 0)
      SendCurrentBuffer();
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

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
void* HookAlloc(const AllocatorDispatch* self, size_t size, void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_function(next, size, context);
  AllocatorShimLogAlloc(AllocatorType::kMalloc, ptr, size, nullptr);
  return ptr;
}

void* HookZeroInitAlloc(const AllocatorDispatch* self,
                        size_t n,
                        size_t size,
                        void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_zero_initialized_function(next, n, size, context);
  AllocatorShimLogAlloc(AllocatorType::kMalloc, ptr, n * size, nullptr);
  return ptr;
}

void* HookAllocAligned(const AllocatorDispatch* self,
                       size_t alignment,
                       size_t size,
                       void* context) {
  const AllocatorDispatch* const next = self->next;
  void* ptr = next->alloc_aligned_function(next, alignment, size, context);
  AllocatorShimLogAlloc(AllocatorType::kMalloc, ptr, size, nullptr);
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
    AllocatorShimLogAlloc(AllocatorType::kMalloc, ptr, size, nullptr);
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
    AllocatorShimLogAlloc(AllocatorType::kMalloc, results[i], size, nullptr);
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
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

void HookPartitionAlloc(void* address, size_t size, const char* type) {
  AllocatorShimLogAlloc(AllocatorType::kPartitionAlloc, address, size, type);
}

void HookPartitionFree(void* address) {
  AllocatorShimLogFree(address);
}

void HookGCAlloc(uint8_t* address, size_t size, const char* type) {
  AllocatorShimLogAlloc(AllocatorType::kOilpan, address, size, type);
}

void HookGCFree(uint8_t* address) {
  AllocatorShimLogFree(address);
}

}  // namespace

void InitTLSSlot() {
  ignore_result(g_prevent_reentrancy.Pointer()->Get());
}

void InitAllocatorShim(MemlogSenderPipe* sender_pipe) {
  // Must be done before hooking any functions that make stack traces.
  base::debug::EnableInProcessStackDumping();

  g_send_buffers = new SendBuffer[kNumSendBuffers];
  g_sender_pipe = sender_pipe;

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  // Normal malloc allocator shim.
  base::allocator::InsertAllocatorDispatch(&g_memlog_hooks);
#endif

  // PartitionAlloc allocator shim.
  base::PartitionAllocHooks::SetAllocationHook(&HookPartitionAlloc);
  base::PartitionAllocHooks::SetFreeHook(&HookPartitionFree);

  // GC (Oilpan) allocator shim.
  if (g_hook_gc_alloc && g_hook_gc_free) {
    g_hook_gc_alloc(&HookGCAlloc);
    g_hook_gc_free(&HookGCFree);
  }
}

void StopAllocatorShimDangerous() {
  g_send_buffers = nullptr;
  base::PartitionAllocHooks::SetAllocationHook(nullptr);
  base::PartitionAllocHooks::SetFreeHook(nullptr);

  if (g_hook_gc_alloc && g_hook_gc_free) {
    g_hook_gc_alloc(nullptr);
    g_hook_gc_free(nullptr);
  }
}

void AllocatorShimLogAlloc(AllocatorType type,
                           void* address,
                           size_t sz,
                           const char* context) {
  if (!g_send_buffers)
    return;
  if (UNLIKELY(g_prevent_reentrancy.Pointer()->Get()))
    return;
  g_prevent_reentrancy.Pointer()->Set(true);

  if (address) {
    constexpr size_t max_message_size = sizeof(AllocPacket) +
                                        kMaxStackEntries * sizeof(uint64_t) +
                                        kMaxContextLen;
    static_assert(max_message_size < kSendBufferSize,
                  "We can't have a message size that exceeds the pipe write "
                  "buffer size.");
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

    size_t context_len = context ? strnlen(context, kMaxContextLen) : 0;

    alloc_packet->op = kAllocPacketType;
    alloc_packet->allocator = type;
    alloc_packet->address = (uint64_t)address;
    alloc_packet->size = sz;
    alloc_packet->stack_len = static_cast<uint32_t>(frame_count);
    alloc_packet->context_byte_len = static_cast<uint32_t>(context_len);

    char* message_end = reinterpret_cast<char*>(&stack[frame_count]);
    if (context_len > 0) {
      memcpy(message_end, context, context_len);
      message_end += context_len;
    }

    DoSend(address, message, message_end - message);
  }

  g_prevent_reentrancy.Pointer()->Set(false);
}

void AllocatorShimLogFree(void* address) {
  if (!g_send_buffers)
    return;
  if (UNLIKELY(g_prevent_reentrancy.Pointer()->Get()))
    return;
  g_prevent_reentrancy.Pointer()->Set(true);

  if (address) {
    FreePacket free_packet;
    free_packet.op = kFreePacketType;
    free_packet.address = (uint64_t)address;

    DoSend(address, &free_packet, sizeof(FreePacket));
  }

  g_prevent_reentrancy.Pointer()->Set(false);
}

void AllocatorShimFlushPipe(uint32_t barrier_id) {
  if (!g_send_buffers)
    return;
  for (int i = 0; i < kNumSendBuffers; i++)
    g_send_buffers[i].Flush();

  BarrierPacket barrier;
  barrier.barrier_id = barrier_id;
  g_sender_pipe->Send(&barrier, sizeof(barrier));
}

void SetGCHeapAllocationHookFunctions(SetGCAllocHookFunction hook_alloc,
                                      SetGCFreeHookFunction hook_free) {
  g_hook_gc_alloc = hook_alloc;
  g_hook_gc_free = hook_free;

  if (g_sender_pipe) {
    // If starting the memlog pipe beat Blink initialization, hook the
    // functions now.
    g_hook_gc_alloc(&HookGCAlloc);
    g_hook_gc_free(&HookGCFree);
  }
}

}  // namespace profiling
