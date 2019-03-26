// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <windows.h>

#include <stddef.h>
#include <winternl.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/unwind_result.h"
#include "base/profiler/win32_stack_frame_unwinder.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/stl_util.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "base/win/scoped_handle.h"
#include "build/build_config.h"

namespace base {

// Stack recording functions --------------------------------------------------

namespace {

// The thread environment block internal type.
struct TEB {
  NT_TIB Tib;
  // Rest of struct is ignored.
};

// Returns the thread environment block pointer for |thread_handle|.
const TEB* GetThreadEnvironmentBlock(HANDLE thread_handle) {
  // Define the internal types we need to invoke NtQueryInformationThread.
  enum THREAD_INFORMATION_CLASS { ThreadBasicInformation };

  struct CLIENT_ID {
    HANDLE UniqueProcess;
    HANDLE UniqueThread;
  };

  struct THREAD_BASIC_INFORMATION {
    NTSTATUS ExitStatus;
    TEB* Teb;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    LONG Priority;
    LONG BasePriority;
  };

  using NtQueryInformationThreadFunction =
      NTSTATUS(WINAPI*)(HANDLE, THREAD_INFORMATION_CLASS, PVOID, ULONG, PULONG);

  const auto nt_query_information_thread =
      reinterpret_cast<NtQueryInformationThreadFunction>(::GetProcAddress(
          ::GetModuleHandle(L"ntdll.dll"), "NtQueryInformationThread"));
  if (!nt_query_information_thread)
    return nullptr;

  THREAD_BASIC_INFORMATION basic_info = {0};
  NTSTATUS status = nt_query_information_thread(
      thread_handle, ThreadBasicInformation, &basic_info,
      sizeof(THREAD_BASIC_INFORMATION), nullptr);
  if (status != 0)
    return nullptr;

  return basic_info.Teb;
}

// If the value at |pointer| points to the original stack, rewrite it to point
// to the corresponding location in the copied stack.
//
// IMPORTANT NOTE: This function is invoked while the target thread is
// suspended so it must not do any allocation from the default heap, including
// indirectly via use of DCHECK/CHECK or other logging statements. Otherwise
// this code can deadlock on heap locks in the default heap acquired by the
// target thread before it was suspended.
uintptr_t RewritePointerIfInOriginalStack(
    const uintptr_t* original_stack_bottom,
    const uintptr_t* original_stack_top,
    const uintptr_t* stack_copy_bottom,
    uintptr_t pointer) {
  auto original_stack_bottom_uint =
      reinterpret_cast<uintptr_t>(original_stack_bottom);
  auto original_stack_top_uint =
      reinterpret_cast<uintptr_t>(original_stack_top);
  auto stack_copy_bottom_uint = reinterpret_cast<uintptr_t>(stack_copy_bottom);

  if (pointer < original_stack_bottom_uint ||
      pointer >= original_stack_top_uint)
    return pointer;

  return stack_copy_bottom_uint + (pointer - original_stack_bottom_uint);
}

// Copies the stack to a buffer while rewriting possible pointers to locations
// within the stack to point to the corresponding locations in the copy. This is
// necessary to handle stack frames with dynamic stack allocation, where a
// pointer to the beginning of the dynamic allocation area is stored on the
// stack and/or in a non-volatile register.
//
// Eager rewriting of anything that looks like a pointer to the stack, as done
// in this function, does not adversely affect the stack unwinding. The only
// other values on the stack the unwinding depends on are return addresses,
// which should not point within the stack memory. The rewriting is guaranteed
// to catch all pointers because the stacks are guaranteed by the ABI to be
// sizeof(uintptr_t*) aligned.
//
// IMPORTANT NOTE: This function is invoked while the target thread is
// suspended so it must not do any allocation from the default heap, including
// indirectly via use of DCHECK/CHECK or other logging statements. Otherwise
// this code can deadlock on heap locks in the default heap acquired by the
// target thread before it was suspended.
void CopyStackContentsAndRewritePointers(const uintptr_t* original_stack_bottom,
                                         const uintptr_t* original_stack_top,
                                         uintptr_t* stack_copy_bottom,
                                         CONTEXT* thread_context)
    NO_SANITIZE("address") {
  const uintptr_t* src = original_stack_bottom;
  uintptr_t* dst = stack_copy_bottom;
  for (; src < original_stack_top; ++src, ++dst) {
    *dst = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom, *src);
  }

  // Rewrite pointers in the context.
#if defined(ARCH_CPU_64_BITS)
  DWORD64 CONTEXT::*const nonvolatile_registers[] = {
#if defined(ARCH_CPU_X86_64)
    &CONTEXT::R12,
    &CONTEXT::R13,
    &CONTEXT::R14,
    &CONTEXT::R15,
    &CONTEXT::Rdi,
    &CONTEXT::Rsi,
    &CONTEXT::Rbx,
    &CONTEXT::Rbp,
    &CONTEXT::Rsp
#elif defined(ARCH_CPU_ARM64)
    &CONTEXT::X19,
    &CONTEXT::X20,
    &CONTEXT::X21,
    &CONTEXT::X22,
    &CONTEXT::X23,
    &CONTEXT::X24,
    &CONTEXT::X25,
    &CONTEXT::X26,
    &CONTEXT::X27,
    &CONTEXT::X28,
    &CONTEXT::Fp,
    &CONTEXT::Lr
#else
#error Unsupported Windows 64-bit Arch
#endif
  };

  for (auto reg_field : nonvolatile_registers) {
    DWORD64* const reg = &(thread_context->*reg_field);
    *reg = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom, *reg);
  }
#endif
}

// ScopedDisablePriorityBoost -------------------------------------------------

// Disables priority boost on a thread for the lifetime of the object.
class ScopedDisablePriorityBoost {
 public:
  ScopedDisablePriorityBoost(HANDLE thread_handle);
  ~ScopedDisablePriorityBoost();

 private:
  HANDLE thread_handle_;
  BOOL got_previous_boost_state_;
  BOOL boost_state_was_disabled_;

  DISALLOW_COPY_AND_ASSIGN(ScopedDisablePriorityBoost);
};

ScopedDisablePriorityBoost::ScopedDisablePriorityBoost(HANDLE thread_handle)
    : thread_handle_(thread_handle),
      got_previous_boost_state_(false),
      boost_state_was_disabled_(false) {
  got_previous_boost_state_ =
      ::GetThreadPriorityBoost(thread_handle_, &boost_state_was_disabled_);
  if (got_previous_boost_state_) {
    // Confusingly, TRUE disables priority boost.
    ::SetThreadPriorityBoost(thread_handle_, TRUE);
  }
}

ScopedDisablePriorityBoost::~ScopedDisablePriorityBoost() {
  if (got_previous_boost_state_)
    ::SetThreadPriorityBoost(thread_handle_, boost_state_was_disabled_);
}

// ScopedSuspendThread --------------------------------------------------------

// Suspends a thread for the lifetime of the object.
class ScopedSuspendThread {
 public:
  ScopedSuspendThread(HANDLE thread_handle);
  ~ScopedSuspendThread();

  bool was_successful() const { return was_successful_; }

 private:
  HANDLE thread_handle_;
  bool was_successful_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSuspendThread);
};

ScopedSuspendThread::ScopedSuspendThread(HANDLE thread_handle)
    : thread_handle_(thread_handle),
      was_successful_(::SuspendThread(thread_handle) !=
                      static_cast<DWORD>(-1)) {}

ScopedSuspendThread::~ScopedSuspendThread() {
  if (!was_successful_)
    return;

  // Disable the priority boost that the thread would otherwise receive on
  // resume. We do this to avoid artificially altering the dynamics of the
  // executing application any more than we already are by suspending and
  // resuming the thread.
  //
  // Note that this can racily disable a priority boost that otherwise would
  // have been given to the thread, if the thread is waiting on other wait
  // conditions at the time of SuspendThread and those conditions are satisfied
  // before priority boost is reenabled. The measured length of this window is
  // ~100us, so this should occur fairly rarely.
  ScopedDisablePriorityBoost disable_priority_boost(thread_handle_);
  bool resume_thread_succeeded =
      ::ResumeThread(thread_handle_) != static_cast<DWORD>(-1);
  CHECK(resume_thread_succeeded) << "ResumeThread failed: " << GetLastError();
}

// Tests whether |stack_pointer| points to a location in the guard page.
//
// IMPORTANT NOTE: This function is invoked while the target thread is
// suspended so it must not do any allocation from the default heap, including
// indirectly via use of DCHECK/CHECK or other logging statements. Otherwise
// this code can deadlock on heap locks in the default heap acquired by the
// target thread before it was suspended.
bool PointsToGuardPage(uintptr_t stack_pointer) {
  MEMORY_BASIC_INFORMATION memory_info;
  SIZE_T result = ::VirtualQuery(reinterpret_cast<LPCVOID>(stack_pointer),
                                 &memory_info, sizeof(memory_info));
  return result != 0 && (memory_info.Protect & PAGE_GUARD);
}

}  // namespace

// StackSamplerWin ------------------------------------------------------

class StackSamplerWin : public StackSampler {
 public:
  StackSamplerWin(win::ScopedHandle thread_handle,
                  ModuleCache* module_cache,
                  StackSamplerTestDelegate* test_delegate);
  ~StackSamplerWin() override;

  // StackSamplingProfiler::StackSampler:
  void RecordStackFrames(StackBuffer* stack_buffer,
                         ProfileBuilder* profile_builder) override;

 private:
  // Suspends the thread with |thread_handle|, copies its stack base address,
  // stack, and register context, and records the current metadata, then resumes
  // the thread.  Returns true on success, and returns the copied state via the
  // |base_address|, |stack_buffer|, |profile_builder|, and |thread_context|
  // params.
  static bool CopyStack(HANDLE thread_handle,
                        const void* base_address,
                        StackBuffer* stack_buffer,
                        ProfileBuilder* profile_builder,
                        CONTEXT* thread_context);

  // Walks the stack represented by |thread_context|, recording and returning
  // the frames.
  std::vector<ProfileBuilder::Frame> WalkStack(CONTEXT* thread_context);

  // Attempts to walk native frames in the stack represented by
  // |thread_context|, appending frames to |stack|. Returns a result indicating
  // the disposition of the unwinding.
  UnwindResult WalkNativeFrames(CONTEXT* thread_context,
                                std::vector<ProfileBuilder::Frame>* stack);

  win::ScopedHandle thread_handle_;

  ModuleCache* module_cache_;

  StackSamplerTestDelegate* const test_delegate_;

  // The stack base address corresponding to |thread_handle_|.
  const void* const thread_stack_base_address_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplerWin);
};

StackSamplerWin::StackSamplerWin(win::ScopedHandle thread_handle,
                                 ModuleCache* module_cache,
                                 StackSamplerTestDelegate* test_delegate)
    : thread_handle_(thread_handle.Take()),
      module_cache_(module_cache),
      test_delegate_(test_delegate),
      thread_stack_base_address_(
          GetThreadEnvironmentBlock(thread_handle_.Get())->Tib.StackBase) {}

StackSamplerWin::~StackSamplerWin() {}

void StackSamplerWin::RecordStackFrames(StackBuffer* stack_buffer,
                                        ProfileBuilder* profile_builder) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
               "StackSamplerWin::RecordStackFrames");
  DCHECK(stack_buffer);

  CONTEXT thread_context;
  bool success = CopyStack(thread_handle_.Get(), thread_stack_base_address_,
                           stack_buffer, profile_builder, &thread_context);
  if (!success)
    return;

  if (test_delegate_)
    test_delegate_->OnPreStackWalk();

  profile_builder->OnSampleCompleted(WalkStack(&thread_context));
}

// Suspends the thread with |thread_handle|, copies its stack, register context,
// and current metadata and resumes the thread. Returns true on success.
//
// IMPORTANT NOTE: No allocations from the default heap may occur in the
// ScopedSuspendThread scope, including indirectly via use of DCHECK/CHECK or
// other logging statements. Otherwise this code can deadlock on heap locks in
// the default heap acquired by the target thread before it was suspended.
//
// static
bool StackSamplerWin::CopyStack(HANDLE thread_handle,
                                const void* base_address,
                                StackBuffer* stack_buffer,
                                ProfileBuilder* profile_builder,
                                CONTEXT* thread_context) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"),
               "SuspendThread");

  *thread_context = {0};
  thread_context->ContextFlags = CONTEXT_FULL;

  {
    ScopedSuspendThread suspend_thread(thread_handle);

    if (!suspend_thread.was_successful())
      return false;

    if (!::GetThreadContext(thread_handle, thread_context))
      return false;

    const uintptr_t top = reinterpret_cast<uintptr_t>(base_address);

#if defined(ARCH_CPU_X86_64)
    const uintptr_t bottom = thread_context->Rsp;
#elif defined(ARCH_CPU_ARM64)
    const uintptr_t bottom = thread_context->Sp;
#else
    const uintptr_t bottom = thread_context->Esp;
#endif

    if ((top - bottom) > stack_buffer->size())
      return false;

    // Dereferencing a pointer in the guard page in a thread that doesn't own
    // the stack results in a STATUS_GUARD_PAGE_VIOLATION exception and a
    // crash. This occurs very rarely, but reliably over the population.
    if (PointsToGuardPage(bottom))
      return false;

    profile_builder->RecordMetadata();

    CopyStackContentsAndRewritePointers(reinterpret_cast<uintptr_t*>(bottom),
                                        reinterpret_cast<uintptr_t*>(top),
                                        stack_buffer->buffer(), thread_context);
  }

  return true;
}

std::vector<ProfileBuilder::Frame> StackSamplerWin::WalkStack(
    CONTEXT* thread_context) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler.debug"), "WalkStack");
  std::vector<ProfileBuilder::Frame> stack;
  // Reserve enough memory for most stacks, to avoid repeated
  // allocations. Approximately 99.9% of recorded stacks are 128 frames or
  // fewer.
  stack.reserve(128);

  WalkNativeFrames(thread_context, &stack);

  return stack;
}

UnwindResult StackSamplerWin::WalkNativeFrames(
    CONTEXT* thread_context,
    std::vector<ProfileBuilder::Frame>* stack) {
  Win32StackFrameUnwinder frame_unwinder;
  while (ContextPC(thread_context)) {
    const ModuleCache::Module* const module =
        module_cache_->GetModuleForAddress(ContextPC(thread_context));

    if (!module) {
      // There's no loaded module containing the instruction pointer. This can
      // be due to executing code that is not in a module (e.g. V8 generated
      // code or runtime-generated code associated with third-party injected
      // DLLs). It can also be due to the the module having been unloaded since
      // we recorded the stack.  In the latter case the function unwind
      // information was part of the unloaded module, so it's not possible to
      // unwind further.
      //
      // If a module was found, it's still theoretically possible for the
      // detected module module to be different than the one that was loaded
      // when the stack was copied (i.e. if the module was unloaded and a
      // different module loaded in overlapping memory). This likely would cause
      // a crash, but has not been observed in practice.
      //
      // We return UNRECOGNIZED_FRAME on the optimistic assumption that this may
      // be a frame the AuxUnwinder knows how to handle (e.g. a frame in V8
      // generated code).
      return UnwindResult::UNRECOGNIZED_FRAME;
    }

    // Record the current frame.
    stack->emplace_back(ContextPC(thread_context), module);

    if (!frame_unwinder.TryUnwind(thread_context, module))
      return UnwindResult::ABORTED;
  }

  return UnwindResult::COMPLETED;
}

// StackSampler ---------------------------------------------------------

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    PlatformThreadId thread_id,
    ModuleCache* module_cache,
    StackSamplerTestDelegate* test_delegate) {
#if _WIN64
  // Get the thread's handle.
  HANDLE thread_handle = ::OpenThread(
      THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME | THREAD_QUERY_INFORMATION,
      FALSE, thread_id);

  if (thread_handle) {
    return std::make_unique<StackSamplerWin>(win::ScopedHandle(thread_handle),
                                             module_cache, test_delegate);
  }
#endif
  return nullptr;
}

// static
size_t StackSampler::GetStackBufferSize() {
  // The default Win32 reserved stack size is 1 MB and Chrome Windows threads
  // currently always use the default, but this allows for expansion if it
  // occurs. The size beyond the actual stack size consists of unallocated
  // virtual memory pages so carries little cost (just a bit of wasted address
  // space).
  return 2 << 20;  // 2 MiB
}

}  // namespace base
