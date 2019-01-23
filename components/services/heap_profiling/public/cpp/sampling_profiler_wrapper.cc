// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/heap_profiling/public/cpp/sampling_profiler_wrapper.h"

#include "base/allocator/buildflags.h"
#include "base/atomicops.h"
#include "base/compiler_specific.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/stack_trace.h"
#include "base/lazy_instance.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_id_name_manager.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/heap_profiler_event_filter.h"
#include "base/trace_event/memory_dump_manager.h"
#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#endif

#if defined(OS_LINUX) || defined(OS_ANDROID)
#include <sys/prctl.h>
#endif

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
#include "base/trace_event/cfi_backtrace_android.h"
#endif

using base::trace_event::AllocationContext;
using base::trace_event::AllocationContextTracker;
using CaptureMode = base::trace_event::AllocationContextTracker::CaptureMode;

namespace heap_profiling {

// Allocation logging also requires use of base TLS, so we must also check that
// that is available. This means that allocations that occur after base TLS has
// been torn down will not be logged.
// TODO(alph): Get rid of the class. crbug.com/917476
class ScopedAllowAlloc {
 public:
  static inline bool HasTLSBeenDestroyed() {
    return UNLIKELY(base::ThreadLocalStorage::HasBeenDestroyed());
  }
};

namespace {

bool g_initialized_ = false;
base::LazyInstance<base::Lock>::Leaky g_on_init_allocator_shim_lock_;
base::LazyInstance<base::OnceClosure>::Leaky g_on_init_allocator_shim_callback_;
base::LazyInstance<scoped_refptr<base::TaskRunner>>::Leaky
    g_on_init_allocator_shim_task_runner_;

SenderPipe* g_sender_pipe = nullptr;

// In NATIVE stack mode, whether to insert stack names into the backtraces.
bool g_include_thread_names = false;

// Prime since this is used like a hash table. Numbers of this magnitude seemed
// to provide sufficient parallelism to avoid lock overhead in ad-hoc testing.
constexpr int kNumSendBuffers = 17;

// If writing to the SenderPipe ever takes longer than 10s, just give up.
constexpr int kTimeoutMs = 10000;

// The allocator shim needs to retain some additional state for each thread.
struct ShimState {
  // The pointer must be valid for the lifetime of the process.
  const char* thread_name = nullptr;

  // If we are using pseudo stacks, we need to inform the profiling service of
  // the address to string mapping. To avoid a global lock, we keep a
  // thread-local unordered_set of every address that has been sent from the
  // thread in question.
  std::unordered_set<const void*> sent_strings;
};

// Technically, this code could be called after Thread destruction and we would
// need to guard this with ThreadLocalStorage::HasBeenDestroyed(), but all calls
// to this are guarded behind ScopedAllowAlloc, which already makes the check.
base::ThreadLocalStorage::Slot& ShimStateTLS() {
  static base::NoDestructor<base::ThreadLocalStorage::Slot> shim_state_tls(
      [](void* shim_state) { delete static_cast<ShimState*>(shim_state); });
  return *shim_state_tls;
}

// We don't need to worry about re-entrancy because PoissonAllocationSampler
// already guards against that.
ShimState* GetShimState() {
  ShimState* state = static_cast<ShimState*>(ShimStateTLS().Get());

  if (!state) {
    state = new ShimState();
    ShimStateTLS().Set(state);
  }

  return state;
}

// Set the thread name, which is a pointer to a leaked string, to ensure
// validity forever.
void SetCurrentThreadName(const char* name) {
  GetShimState()->thread_name = name;
}

// If a thread name has been set from ThreadIdNameManager, use that. Otherwise,
// gets the thread name from kernel if available or returns a string with id.
// This function intentionally leaks the allocated strings since they are used
// to tag allocations even after the thread dies.
const char* GetAndLeakThreadName() {
  const char* thread_name =
      base::ThreadIdNameManager::GetInstance()->GetNameForCurrentThread();
  if (thread_name && strcmp(thread_name, "") != 0)
    return thread_name;

  // prctl requires 16 bytes, snprintf requires 19, pthread_getname_np requires
  // 64 on macOS, see PlatformThread::SetName in platform_thread_mac.mm.
  constexpr size_t kBufferLen = 64;
  char name[kBufferLen];
#if defined(OS_LINUX) || defined(OS_ANDROID)
  // If the thread name is not set, try to get it from prctl. Thread name might
  // not be set in cases where the thread started before heap profiling was
  // enabled.
  int err = prctl(PR_GET_NAME, name);
  if (!err) {
    return strdup(name);
  }
#elif defined(OS_MACOSX)
  int err = pthread_getname_np(pthread_self(), name, kBufferLen);
  if (err == 0 && name[0] != '\0') {
    return strdup(name);
  }
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

  // Use tid if we don't have a thread name.
  snprintf(name, sizeof(name), "Thread %lu",
           static_cast<unsigned long>(base::PlatformThread::CurrentId()));
  return strdup(name);
}

// Returns the thread name, looking it up if necessary.
const char* GetOrSetThreadName() {
  const char* thread_name = GetShimState()->thread_name;
  if (UNLIKELY(!thread_name)) {
    thread_name = GetAndLeakThreadName();
    GetShimState()->thread_name = thread_name;
  }
  return thread_name;
}

class SendBuffer {
 public:
  SendBuffer() : buffer_(new char[SenderPipe::kPipeSize]) {}
  ~SendBuffer() { delete[] buffer_; }

  void Send(const void* data, size_t sz) {
    base::AutoLock lock(lock_);

    if (used_ + sz > SenderPipe::kPipeSize)
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
    SenderPipe::Result result = g_sender_pipe->Send(buffer_, used_, kTimeoutMs);
    used_ = 0;
    if (result == SenderPipe::Result::kError) {
      SamplingProfilerWrapper::FlushBuffersAndClosePipe();
    }
    if (result == SenderPipe::Result::kTimeout) {
      SamplingProfilerWrapper::FlushBuffersAndClosePipe();
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

size_t HashAddress(const void* address) {
  // The multiplicative hashing scheme from [Knuth 1998].
  // |a| is the first prime after 2^17.
  const uintptr_t key = reinterpret_cast<uintptr_t>(address);
  const uintptr_t a = 131101;
  const uintptr_t shift = 15;
  const uintptr_t h = (key * a) >> shift;
  return h;
}

// "address" is the address in question, which is used to select which send
// buffer to use.
void DoSend(const void* address,
            const void* data,
            size_t size,
            SendBuffer* send_buffers) {
  int bin_to_use = HashAddress(address) % kNumSendBuffers;
  send_buffers[bin_to_use].Send(data, size);
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

  void AddCString(const char* c_string) {
    // Using a TLS cache of sent_strings avoids lock contention on malloc, which
    // would kill performance.
    std::unordered_set<const void*>* sent_strings =
        &GetShimState()->sent_strings;

    if (sent_strings->find(c_string) == sent_strings->end()) {
      // No point in allowing arbitrarily long c-strings, which might cause pipe
      // max length issues. Pick a reasonable length like 255.
      static const size_t kMaxCStringLen = 255;

      // length does not include the null terminator.
      size_t length = strnlen(c_string, kMaxCStringLen);

      char message[sizeof(StringMappingPacket) + kMaxCStringLen];
      StringMappingPacket* string_mapping_packet =
          new (&message) StringMappingPacket();
      string_mapping_packet->address = reinterpret_cast<uint64_t>(c_string);
      string_mapping_packet->string_len = length;
      memcpy(message + sizeof(StringMappingPacket), c_string, length);
      DoSend(address_, message, sizeof(StringMappingPacket) + length,
             send_buffers_);
      sent_strings->insert(c_string);
    }

    AddInstructionPointer(c_string);
  }

  size_t count() { return count_; }

 private:
  void AddFrame(const base::trace_event::StackFrame& frame) {
    if (frame.type == base::trace_event::StackFrame::Type::PROGRAM_COUNTER) {
      AddInstructionPointer(frame.value);
      return;
    }

    AddCString(static_cast<const char*>(frame.value));
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
  base::PoissonAllocationSampler::Init();
  ignore_result(ShimStateTLS());
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

void InitAllocationRecorder(SenderPipe* sender_pipe,
                            mojom::ProfilingParamsPtr params) {
  // Must be done before hooking any functions that make stack traces.
  base::debug::EnableInProcessStackDumping();

  if (params->stack_mode == mojom::StackMode::NATIVE_WITH_THREAD_NAMES) {
    g_include_thread_names = true;
    base::ThreadIdNameManager::GetInstance()->InstallSetNameCallback(
        base::BindRepeating(&SetCurrentThreadName));
  }

  switch (params->stack_mode) {
    case mojom::StackMode::PSEUDO:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::PSEUDO_STACK);
      break;
    case mojom::StackMode::MIXED:
      EnableTraceEventFiltering();
      AllocationContextTracker::SetCaptureMode(CaptureMode::MIXED_STACK);
      break;
    case mojom::StackMode::NATIVE_WITH_THREAD_NAMES:
    case mojom::StackMode::NATIVE_WITHOUT_THREAD_NAMES:
      // This would track task contexts only.
      AllocationContextTracker::SetCaptureMode(CaptureMode::NATIVE_STACK);
      break;
  }

  g_send_buffers.Write(new SendBuffer[kNumSendBuffers]);
  g_sender_pipe = sender_pipe;
}

void SamplingProfilerWrapper::FlushBuffersAndClosePipe() {
  // This ShareBuffer array is leaked on purpose to avoid races on Stop.
  g_send_buffers.Write(nullptr);
  if (g_sender_pipe)
    g_sender_pipe->Close();
}

void SerializeFramesFromAllocationContext(FrameSerializer* serializer,
                                          const char** context) {
  // Allocation context is tracked in TLS. Return nothing if TLS was destroyed.
  if (ScopedAllowAlloc::HasTLSBeenDestroyed())
    return;
  auto* tracker = AllocationContextTracker::GetInstanceForCurrentThread();
  if (!tracker)
    return;

  AllocationContext allocation_context;
  if (!tracker->GetContextSnapshot(&allocation_context))
    return;

  serializer->AddAllFrames(allocation_context.backtrace);
  if (!*context)
    *context = allocation_context.type_name;
}

void SerializeFramesFromBacktrace(FrameSerializer* serializer,
                                  const char** context) {
  // Skip 3 top frames related to the profiler itself, e.g.:
  //   base::debug::StackTrace::StackTrace
  //   heap_profiling::RecordAndSendAlloc
  //   sampling_heap_profiler::PoissonAllocationSampler::DoRecordAlloc
  size_t skip_frames = 3;
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE) && \
    defined(OFFICIAL_BUILD)
  const void* frames[kMaxStackEntries - 1];
  size_t frame_count =
      base::trace_event::CFIBacktraceAndroid::GetInitializedInstance()->Unwind(
          frames, kMaxStackEntries - 1);
#elif BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
  const void* frames[kMaxStackEntries - 1];
  size_t frame_count = base::debug::TraceStackFramePointers(
      frames, kMaxStackEntries - 1, skip_frames);
  skip_frames = 0;
#else
  // Fall-back to capturing the stack with base::debug::StackTrace,
  // which is likely slower, but more reliable.
  base::debug::StackTrace stack_trace(kMaxStackEntries - 1);
  size_t frame_count = 0u;
  const void* const* frames = stack_trace.Addresses(&frame_count);
#endif

  skip_frames = std::min(skip_frames, frame_count);
  serializer->AddAllInstructionPointers(frame_count - skip_frames,
                                        frames + skip_frames);

  // Both thread name and task context require access to TLS.
  if (ScopedAllowAlloc::HasTLSBeenDestroyed())
    return;

  if (g_include_thread_names) {
    const char* thread_name = GetOrSetThreadName();
    serializer->AddCString(thread_name);
  }

  if (!*context) {
    const auto* tracker =
        AllocationContextTracker::GetInstanceForCurrentThread();
    if (tracker)
      *context = tracker->TaskContext();
  }
}

// Creates allocation info record, populates it with current call stack,
// thread name, allocator type and sends out to the client. Safe to call this
// method after TLS is destroyed.
void RecordAndSendAlloc(AllocatorType type,
                        void* address,
                        size_t sz,
                        const char* context) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;

  constexpr size_t max_message_size = sizeof(AllocPacket) +
                                      kMaxStackEntries * sizeof(uint64_t) +
                                      kMaxContextLen;
  static_assert(max_message_size < SenderPipe::kPipeSize,
                "We can't have a message size that exceeds the pipe write "
                "buffer size.");
  char message[max_message_size];
  // TODO(ajwong) check that this is technically valid.
  AllocPacket* alloc_packet = reinterpret_cast<AllocPacket*>(message);

  uint64_t* stack = reinterpret_cast<uint64_t*>(&message[sizeof(AllocPacket)]);

  FrameSerializer serializer(
      stack, address, max_message_size - sizeof(AllocPacket), send_buffers);

  CaptureMode capture_mode = AllocationContextTracker::capture_mode();
  if (capture_mode == CaptureMode::PSEUDO_STACK ||
      capture_mode == CaptureMode::MIXED_STACK) {
    SerializeFramesFromAllocationContext(&serializer, &context);
  } else {
    SerializeFramesFromBacktrace(&serializer, &context);
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

// Creates the record for free operation and sends it out to the client. Safe
// to call this method after TLS is destroyed.
void RecordAndSendFree(void* address) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;

  FreePacket free_packet;
  free_packet.op = kFreePacketType;
  free_packet.address = (uint64_t)address;

  DoSend(address, &free_packet, sizeof(FreePacket), send_buffers);
}

void SamplingProfilerWrapper::FlushPipe(uint32_t barrier_id) {
  SendBuffer* send_buffers = g_send_buffers.Read();
  if (!send_buffers)
    return;
  for (int i = 0; i < kNumSendBuffers; i++)
    send_buffers[i].Flush();

  BarrierPacket barrier;
  barrier.barrier_id = barrier_id;
  SenderPipe::Result result =
      g_sender_pipe->Send(&barrier, sizeof(barrier), kTimeoutMs);
  if (result != SenderPipe::Result::kSuccess) {
    FlushBuffersAndClosePipe();
    // TODO(erikchen): Emit a histogram. https://crbug.com/777546.
  }
}

bool SetOnInitAllocatorShimCallbackForTesting(
    base::OnceClosure callback,
    scoped_refptr<base::TaskRunner> task_runner) {
  base::AutoLock lock(g_on_init_allocator_shim_lock_.Get());
  if (g_initialized_)
    return true;
  g_on_init_allocator_shim_callback_.Get() = std::move(callback);
  g_on_init_allocator_shim_task_runner_.Get() = task_runner;
  return false;
}

namespace {

// Notifies the test clients that allocation hooks have been initialized.
void AllocatorHooksHaveBeenInitialized() {
  base::AutoLock lock(g_on_init_allocator_shim_lock_.Get());
  g_initialized_ = true;
  if (!g_on_init_allocator_shim_callback_.Get())
    return;
  g_on_init_allocator_shim_task_runner_.Get()->PostTask(
      FROM_HERE, std::move(*g_on_init_allocator_shim_callback_.Pointer()));
}

AllocatorType ConvertType(base::PoissonAllocationSampler::AllocatorType type) {
  static_assert(static_cast<uint32_t>(
                    base::PoissonAllocationSampler::AllocatorType::kMax) ==
                    static_cast<uint32_t>(AllocatorType::kCount),
                "AllocatorType lengths do not match.");
  switch (type) {
    case base::PoissonAllocationSampler::AllocatorType::kMalloc:
      return AllocatorType::kMalloc;
    case base::PoissonAllocationSampler::AllocatorType::kPartitionAlloc:
      return AllocatorType::kPartitionAlloc;
    case base::PoissonAllocationSampler::AllocatorType::kBlinkGC:
      return AllocatorType::kOilpan;
    default:
      NOTREACHED();
      return AllocatorType::kMalloc;
  }
}

}  // namespace

SamplingProfilerWrapper::SamplingProfilerWrapper() {
  base::PoissonAllocationSampler::Get()->AddSamplesObserver(this);
}

SamplingProfilerWrapper::~SamplingProfilerWrapper() {
  base::PoissonAllocationSampler::Get()->RemoveSamplesObserver(this);
}

void SamplingProfilerWrapper::StartProfiling(SenderPipe* sender_pipe,
                                             mojom::ProfilingParamsPtr params) {
  size_t sampling_rate = params->sampling_rate;
  InitAllocationRecorder(sender_pipe, std::move(params));
  auto* sampler = base::PoissonAllocationSampler::Get();
  sampler->SetSamplingInterval(sampling_rate);
  sampler->Start();
  AllocatorHooksHaveBeenInitialized();
}

void SamplingProfilerWrapper::StopProfiling() {
  base::PoissonAllocationSampler::Get()->Stop();
}

void SamplingProfilerWrapper::SampleAdded(
    void* address,
    size_t size,
    size_t total,
    base::PoissonAllocationSampler::AllocatorType type,
    const char* context) {
  RecordAndSendAlloc(ConvertType(type), address, size, context);
}

void SamplingProfilerWrapper::SampleRemoved(void* address) {
  RecordAndSendFree(address);
}

}  // namespace heap_profiling
