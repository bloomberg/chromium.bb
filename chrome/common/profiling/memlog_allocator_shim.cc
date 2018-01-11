// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/profiling/memlog_allocator_shim.h"

#include "base/allocator/allocator_shim.h"
#include "base/allocator/features.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/atomicops.h"
#include "base/compiler_specific.h"
#include "base/debug/debugging_flags.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/numerics/safe_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_local.h"
#include "base/trace_event/heap_profiler_allocation_context_tracker.h"
#include "base/trace_event/heap_profiler_allocation_register.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"
#include "chrome/common/profiling/memlog_stream.h"

#if defined(OS_POSIX)
#include <limits.h>
#endif

using base::trace_event::AllocationContext;
using base::trace_event::AllocationContextTracker;
using CaptureMode = base::trace_event::AllocationContextTracker::CaptureMode;

namespace profiling {

namespace {

using base::allocator::AllocatorDispatch;

base::LazyInstance<base::OnceClosure>::Leaky g_on_init_allocator_shim_callback_;
base::LazyInstance<scoped_refptr<base::TaskRunner>>::Leaky
    g_on_init_allocator_shim_task_runner_;

MemlogSenderPipe* g_sender_pipe = nullptr;

// Prime since this is used like a hash table. Numbers of this magnitude seemed
// to provide sufficient parallelism to avoid lock overhead in ad-hoc testing.
constexpr int kNumSendBuffers = 17;

// If writing to the MemlogSenderPipe ever takes longer than 10s, just give up.
constexpr int kTimeoutMs = 10000;

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

// If we are using pseudo stacks, we need to inform the profiling service of the
// address to string mapping. To avoid a global lock, we keep a thread-local
// unordered_set of every address that has been sent from the thread in
// question.
base::LazyInstance<base::ThreadLocalPointer<std::unordered_set<const void*>>>::
    Leaky g_sent_strings = LAZY_INSTANCE_INITIALIZER;

class SendBuffer {
 public:
  SendBuffer() : buffer_(new char[MemlogSenderPipe::kPipeSize]) {}
  ~SendBuffer() { delete[] buffer_; }

  void Send(const void* data, size_t sz) {
    base::AutoLock lock(lock_);

    if (used_ + sz > MemlogSenderPipe::kPipeSize)
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
    MemlogSenderPipe::Result result =
        g_sender_pipe->Send(buffer_, used_, kTimeoutMs);
    used_ = 0;
    if (result == MemlogSenderPipe::Result::kError)
      StopAllocatorShimDangerous();
    if (result == MemlogSenderPipe::Result::kTimeout) {
      StopAllocatorShimDangerous();
      // TODO(erikchen): Emit a histogram. https://crbug.com/777546.
    }
  }

  base::Lock lock_;

  char* buffer_;
  size_t used_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SendBuffer);
};

// It's safe to call Read() before Write(). Read() will either return nullptr or
// a valid SendBuffer.
class AtomicallyConsistentSendBufferArray {
 public:
  void Write(SendBuffer* buffer) {
    base::subtle::Release_Store(
        &send_buffers, reinterpret_cast<base::subtle::AtomicWord>(buffer));
  }

  SendBuffer* Read() {
    return reinterpret_cast<SendBuffer*>(
        base::subtle::Acquire_Load(&send_buffers));
  }

 private:
  // This class is used as a static global. This will be linker-initialized to
  // 0.
  base::subtle::AtomicWord send_buffers;
};

// The API guarantees that Read() will either return a valid object or a
// nullptr.
AtomicallyConsistentSendBufferArray g_send_buffers;

// "address" is the address in question, which is used to select which send
// buffer to use.
void DoSend(const void* address,
            const void* data,
            size_t size,
            SendBuffer* send_buffers) {
  base::trace_event::AllocationRegister::AddressHasher hasher;
  int bin_to_use = hasher(address) % kNumSendBuffers;
  send_buffers[bin_to_use].Send(data, size);
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

// Updates an existing in_memory buffer with frame data. If a frame contains a
// pointer to a cstring rather than an instruction pointer, and the profiling
// service has not yet been informed of that pointer -> cstring mapping, sends a
// StringMappingPacket.
class FrameSerializer {
 public:
  FrameSerializer(uint64_t* stack,
                  const void* address,
                  size_t initial_buffer_size,
                  SendBuffer* send_buffers)
      : stack_(stack),
        address_(address),
        remaining_buffer_size_(initial_buffer_size),
        send_buffers_(send_buffers) {}

  void AddAllFrames(const base::trace_event::Backtrace& backtrace) {
    CHECK_LE(backtrace.frame_count, kMaxStackEntries);
    size_t required_capacity = backtrace.frame_count * sizeof(uint64_t);
    CHECK_LE(required_capacity, remaining_buffer_size_);
    remaining_buffer_size_ -= required_capacity;
    for (int i = base::checked_cast<int>(backtrace.frame_count) - 1; i >= 0;
         --i) {
      AddFrame(backtrace.frames[i]);
    }
  }

  void AddAllInstructionPointers(size_t frame_count,
                                 const void* const* frames) {
    CHECK_LE(frame_count, kMaxStackEntries);
    size_t required_capacity = frame_count * sizeof(uint64_t);
    CHECK_LE(required_capacity, remaining_buffer_size_);
    remaining_buffer_size_ -= required_capacity;
    // If there are too many frames, keep the ones furthest from main().
    for (size_t i = 0; i < frame_count; i++)
      AddInstructionPointer(frames[i]);
  }

  size_t count() { return count_; }

 private:
  void AddFrame(const base::trace_event::StackFrame& frame) {
    if (frame.type == base::trace_event::StackFrame::Type::PROGRAM_COUNTER) {
      AddInstructionPointer(frame.value);
      return;
    }

    // Using a TLS cache of sent_strings avoids lock contention on malloc, which
    // would kill performance.
    std::unordered_set<const void*>* sent_strings =
        g_sent_strings.Pointer()->Get();

    if (sent_strings == nullptr) {
      sent_strings = new std::unordered_set<const void*>;
      g_sent_strings.Pointer()->Set(sent_strings);
    }

    if (sent_strings->find(frame.value) == sent_strings->end()) {
      // No point in allowing arbitrarily long c-strings, which might cause pipe
      // max length issues. Pick a reasonable length like 255.
      static const size_t kMaxCStringLen = 255;
      const char* null_terminated_cstring =
          static_cast<const char*>(frame.value);
      // length does not include the null terminator.
      size_t length = strnlen(null_terminated_cstring, kMaxCStringLen);

      char message[sizeof(StringMappingPacket) + kMaxCStringLen];
      StringMappingPacket* string_mapping_packet =
          new (&message) StringMappingPacket();
      string_mapping_packet->address = reinterpret_cast<uint64_t>(frame.value);
      string_mapping_packet->string_len = length;
      memcpy(message + sizeof(StringMappingPacket), null_terminated_cstring,
             length);
      DoSend(address_, message, sizeof(StringMappingPacket) + length,
             send_buffers_);
      sent_strings->insert(frame.value);
    }

    AddInstructionPointer(frame.value);
  }

  void AddInstructionPointer(const void* value) {
    *stack_ = reinterpret_cast<uint64_t>(value);
    ++stack_;
    ++count_;
  }

  // The next frame should be written to this memory location. There are both
  // static and runtime checks to prevent buffer overrun.
  static_assert(
      base::trace_event::Backtrace::kMaxFrameCount < kMaxStackEntries,
      "Ensure that pseudo-stack frame count won't exceed OOP HP frame buffer.");
  uint64_t* stack_;

  // The number of frames that have been written to the stack.
  size_t count_ = 0;

  const void* address_;
  size_t remaining_buffer_size_;
  SendBuffer* send_buffers_;
};

}  // namespace

void InitTLSSlot() {
  ignore_result(g_prevent_reentrancy.Pointer()->Get());
}

// In order for pseudo stacks to work, trace event filtering must be enabled.
void EnableTraceEventFiltering() {
  std::string filter_string = base::JoinString(
      {"*", TRACE_DISABLED_BY_DEFAULT("net"), TRACE_DISABLED_BY_DEFAULT("cc"),
       base::trace_event::MemoryDumpManager::kTraceCategory},
      ",");
  base::trace_event::TraceConfigCategoryFilter category_filter;
  category_filter.InitializeFromString(filter_string);

  base::trace_event::TraceConfig::EventFilterConfig heap_profiler_filter_config(
      base::trace_event::HeapProfilerEventFilter::kName);
  heap_profiler_filter_config.SetCategoryFilter(category_filter);

  base::trace_event::TraceConfig::EventFilters filters;
  filters.push_back(heap_profiler_filter_config);
  base::trace_event::TraceConfig filtering_trace_config;
  filtering_trace_config.SetEventFilters(filters);

  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      filtering_trace_config, base::trace_event::TraceLog::FILTERING_MODE);
}

void InitAllocatorShim(MemlogSenderPipe* sender_pipe,
                       mojom::StackMode stack_mode) {
  // Must be done before hooking any functions that make stack traces.
  base::debug::EnableInProcessStackDumping();

  switch (stack_mode) {
    case mojom::StackMode::PSEUDO:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::PSEUDO_STACK);
      break;
    case mojom::StackMode::MIXED:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::MIXED_STACK);
      break;
    case mojom::StackMode::NATIVE:
      AllocationContextTracker::SetCaptureMode(CaptureMode::DISABLED);
      break;
  }

  g_send_buffers.Write(new SendBuffer[kNumSendBuffers]);
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

  if (*g_on_init_allocator_shim_callback_.Pointer()) {
    (*g_on_init_allocator_shim_task_runner_.Pointer())
        ->PostTask(FROM_HERE,
                   std::move(*g_on_init_allocator_shim_callback_.Pointer()));
  }
}

void StopAllocatorShimDangerous() {
  // This ShareBuffer array is leaked on purpose to avoid races on Stop.
  g_send_buffers.Write(nullptr);

  base::PartitionAllocHooks::SetAllocationHook(nullptr);
  base::PartitionAllocHooks::SetFreeHook(nullptr);

  if (g_hook_gc_alloc && g_hook_gc_free) {
    g_hook_gc_alloc(nullptr);
    g_hook_gc_free(nullptr);
  }

  if (g_sender_pipe)
    g_sender_pipe->Close();
}

void SerializeFramesFromAllocationContext(FrameSerializer* serializer) {
  auto* tracker = AllocationContextTracker::GetInstanceForCurrentThread();
  if (!tracker)
    return;

  AllocationContext allocation_context;
  if (!tracker->GetContextSnapshot(&allocation_context))
    return;

  serializer->AddAllFrames(allocation_context.backtrace);
}

void SerializeFramesFromBacktrace(FrameSerializer* serializer) {
#if BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  const void* frames[kMaxStackEntries];
  size_t frame_count = base::debug::TraceStackFramePointers(
      frames, kMaxStackEntries,
      1);  // exclude this function from the trace.
#else      // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  // Fall-back to capturing the stack with base::debug::StackTrace,
  // which is likely slower, but more reliable.
  base::debug::StackTrace stack_trace(kMaxStackEntries);
  size_t frame_count = 0u;
  const void* const* frames = stack_trace.Addresses(&frame_count);
#endif     // BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)

  serializer->AddAllInstructionPointers(frame_count, frames);
}

void AllocatorShimLogAlloc(AllocatorType type,
                           void* address,
                           size_t sz,
                           const char* context) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;
  if (UNLIKELY(g_prevent_reentrancy.Pointer()->Get()))
    return;
  g_prevent_reentrancy.Pointer()->Set(true);

  if (address) {
    constexpr size_t max_message_size = sizeof(AllocPacket) +
                                        kMaxStackEntries * sizeof(uint64_t) +
                                        kMaxContextLen;
    static_assert(max_message_size < MemlogSenderPipe::kPipeSize,
                  "We can't have a message size that exceeds the pipe write "
                  "buffer size.");
    char message[max_message_size];
    // TODO(ajwong) check that this is technically valid.
    AllocPacket* alloc_packet = reinterpret_cast<AllocPacket*>(message);

    uint64_t* stack =
        reinterpret_cast<uint64_t*>(&message[sizeof(AllocPacket)]);

    FrameSerializer serializer(
        stack, address, max_message_size - sizeof(AllocPacket), send_buffers);

    CaptureMode capture_mode = AllocationContextTracker::capture_mode();
    if (capture_mode == CaptureMode::PSEUDO_STACK ||
        capture_mode == CaptureMode::MIXED_STACK) {
      SerializeFramesFromAllocationContext(&serializer);
    } else {
      SerializeFramesFromBacktrace(&serializer);
    }

    size_t context_len = context ? strnlen(context, kMaxContextLen) : 0;

    alloc_packet->op = kAllocPacketType;
    alloc_packet->allocator = type;
    alloc_packet->address = (uint64_t)address;
    alloc_packet->size = sz;
    alloc_packet->stack_len = static_cast<uint32_t>(serializer.count());
    alloc_packet->context_byte_len = static_cast<uint32_t>(context_len);

    char* message_end = message + sizeof(AllocPacket) +
                        alloc_packet->stack_len * sizeof(uint64_t);
    if (context_len > 0) {
      memcpy(message_end, context, context_len);
      message_end += context_len;
    }
    DoSend(address, message, message_end - message, send_buffers);
  }

  g_prevent_reentrancy.Pointer()->Set(false);
}

void AllocatorShimLogFree(void* address) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;
  if (UNLIKELY(g_prevent_reentrancy.Pointer()->Get()))
    return;
  g_prevent_reentrancy.Pointer()->Set(true);

  if (address) {
    FreePacket free_packet;
    free_packet.op = kFreePacketType;
    free_packet.address = (uint64_t)address;

    DoSend(address, &free_packet, sizeof(FreePacket), send_buffers);
  }

  g_prevent_reentrancy.Pointer()->Set(false);
}

void AllocatorShimFlushPipe(uint32_t barrier_id) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;
  for (int i = 0; i < kNumSendBuffers; i++)
    send_buffers[i].Flush();

  BarrierPacket barrier;
  barrier.barrier_id = barrier_id;
  MemlogSenderPipe::Result result =
      g_sender_pipe->Send(&barrier, sizeof(barrier), kTimeoutMs);
  if (result != MemlogSenderPipe::Result::kSuccess) {
    StopAllocatorShimDangerous();
    // TODO(erikchen): Emit a histogram. https://crbug.com/777546.
  }
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

void SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  *g_on_init_allocator_shim_callback_.Pointer() = std::move(callback);
  *g_on_init_allocator_shim_task_runner_.Pointer() = task_runner;
}

void DisableAllocationTrackingForCurrentThreadForTesting() {
  g_prevent_reentrancy.Pointer()->Set(true);
}

void EnableAllocationTrackingForCurrentThreadForTesting() {
  g_prevent_reentrancy.Pointer()->Set(false);
}

}  // namespace profiling
