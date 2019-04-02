// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/thread_delegate_mac.h"

#include <libkern/OSByteOrder.h>
#include <libunwind.h>
#include <mach-o/compact_unwind_encoding.h>
#include <mach-o/getsect.h>
#include <mach-o/swap.h>
#include <mach/kern_return.h>
#include <mach/mach.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#include <pthread.h>
#include <sys/ptrace.h>
#include <sys/resource.h>
#include <sys/syslimits.h>

#include <algorithm>
#include <memory>

#include "base/logging.h"
#include "base/mac/mach_logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/profile_builder.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/strings/string_number_conversions.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

extern "C" {
void _sigtramp(int, int, struct sigset*);
}

namespace base {

namespace {

// Fills |state| with |target_thread|'s context. NO HEAP ALLOCATIONS.
bool GetThreadState(thread_act_t target_thread, x86_thread_state64_t* state) {
  auto count = static_cast<mach_msg_type_number_t>(x86_THREAD_STATE64_COUNT);
  return thread_get_state(target_thread, x86_THREAD_STATE64,
                          reinterpret_cast<thread_state_t>(state),
                          &count) == KERN_SUCCESS;
}

// Extracts the "frame offset" for a given frame from the compact unwind info.
// A frame offset indicates the location of saved non-volatile registers in
// relation to the frame pointer. See |mach-o/compact_unwind_encoding.h| for
// details.
uint32_t GetFrameOffset(int compact_unwind_info) {
  // The frame offset lives in bytes 16-23. This shifts it down by the number of
  // leading zeroes in the mask, then masks with (1 << number of one bits in the
  // mask) - 1, turning 0x00FF0000 into 0x000000FF. Adapted from |EXTRACT_BITS|
  // in libunwind's CompactUnwinder.hpp.
  return (
      (compact_unwind_info >> __builtin_ctz(UNWIND_X86_64_RBP_FRAME_OFFSET)) &
      (((1 << __builtin_popcount(UNWIND_X86_64_RBP_FRAME_OFFSET))) - 1));
}

// True if the unwind from |leaf_frame_module| may trigger a crash bug in
// unw_init_local. If so, the stack walk should be aborted at the leaf frame.
bool MayTriggerUnwInitLocalCrash(const ModuleCache::Module* leaf_frame_module) {
  // The issue here is a bug in unw_init_local that, in some unwinds, results in
  // attempts to access memory at the address immediately following the address
  // range of the library. When the library is the last of the mapped libraries
  // that address is in a different memory region. Starting with 10.13.4 beta
  // releases it appears that this region is sometimes either unmapped or mapped
  // without read access, resulting in crashes on the attempted access. It's not
  // clear what circumstances result in this situation; attempts to reproduce on
  // a 10.13.4 beta did not trigger the issue.
  //
  // The workaround is to check if the memory address that would be accessed is
  // readable, and if not, abort the stack walk before calling unw_init_local.
  // As of 2018/03/19 about 0.1% of non-idle stacks on the UI and GPU main
  // threads have a leaf frame in the last library. Since the issue appears to
  // only occur some of the time it's expected that the quantity of lost samples
  // will be lower than 0.1%, possibly significantly lower.
  //
  // TODO(lgrey): Add references above to LLVM/Radar bugs on unw_init_local once
  // filed.
  uint64_t unused;
  vm_size_t size = sizeof(unused);
  return vm_read_overwrite(
             current_task(),
             leaf_frame_module->GetBaseAddress() + leaf_frame_module->GetSize(),
             sizeof(unused), reinterpret_cast<vm_address_t>(&unused),
             &size) != 0;
}

// Check if the cursor contains a valid-looking frame pointer for frame pointer
// unwinds. If the stack frame has a frame pointer, stepping the cursor will
// involve indexing memory access off of that pointer. In that case,
// sanity-check the frame pointer register to ensure it's within bounds.
//
// Additionally, the stack frame might be in a prologue or epilogue, which can
// cause a crash when the unwinder attempts to access non-volatile registers
// that have not yet been pushed, or have already been popped from the
// stack. libwunwind will try to restore those registers using an offset from
// the frame pointer. However, since we copy the stack from RSP up, any
// locations below the stack pointer are before the beginning of the stack
// buffer. Account for this by checking that the expected location is above the
// stack pointer, and rejecting the sample if it isn't.
bool HasValidRbp(unw_cursor_t* unwind_cursor, uintptr_t stack_top) {
  unw_proc_info_t proc_info;
  unw_get_proc_info(unwind_cursor, &proc_info);
  if ((proc_info.format & UNWIND_X86_64_MODE_MASK) ==
      UNWIND_X86_64_MODE_RBP_FRAME) {
    unw_word_t rsp, rbp;
    unw_get_reg(unwind_cursor, UNW_X86_64_RSP, &rsp);
    unw_get_reg(unwind_cursor, UNW_X86_64_RBP, &rbp);
    uint32_t offset = GetFrameOffset(proc_info.format) * sizeof(unw_word_t);
    if (rbp < offset || (rbp - offset) < rsp || rbp > stack_top)
      return false;
  }
  return true;
}

const ModuleCache::Module* GetLibSystemKernelModule(ModuleCache* module_cache) {
  const ModuleCache::Module* module =
      module_cache->GetModuleForAddress(reinterpret_cast<uintptr_t>(ptrace));
  DCHECK(module);
  DCHECK_EQ(FilePath("libsystem_kernel.dylib"), module->GetDebugBasename());
  return module;
}

void GetSigtrampRange(uintptr_t* start, uintptr_t* end) {
  auto address = reinterpret_cast<uintptr_t>(&_sigtramp);
  DCHECK(address != 0);

  *start = address;

  unw_context_t context;
  unw_cursor_t cursor;
  unw_proc_info_t info;

  unw_getcontext(&context);
  // Set the context's RIP to the beginning of sigtramp,
  // +1 byte to work around a bug in 10.11 (crbug.com/764468).
  context.data[16] = address + 1;
  unw_init_local(&cursor, &context);
  unw_get_proc_info(&cursor, &info);

  DCHECK_EQ(info.start_ip, address);
  *end = info.end_ip;
}

}  // namespace

// ScopedSuspendThread --------------------------------------------------------

// NO HEAP ALLOCATIONS after thread_suspend.
ThreadDelegateMac::ScopedSuspendThread::ScopedSuspendThread(
    mach_port_t thread_port)
    : thread_port_(thread_suspend(thread_port) == KERN_SUCCESS
                       ? thread_port
                       : MACH_PORT_NULL) {}

// NO HEAP ALLOCATIONS. The MACH_CHECK is OK because it provides a more noisy
// failure mode than deadlocking.
ThreadDelegateMac::ScopedSuspendThread::~ScopedSuspendThread() {
  if (!WasSuccessful())
    return;

  kern_return_t kr = thread_resume(thread_port_);
  MACH_CHECK(kr == KERN_SUCCESS, kr) << "thread_resume";
}

bool ThreadDelegateMac::ScopedSuspendThread::WasSuccessful() const {
  return thread_port_ != MACH_PORT_NULL;
}

// ThreadDelegateMac ----------------------------------------------------------

ThreadDelegateMac::ThreadDelegateMac(mach_port_t thread_port,
                                     ModuleCache* module_cache)
    : thread_port_(thread_port),
      thread_stack_base_address_(reinterpret_cast<uintptr_t>(
          pthread_get_stackaddr_np(pthread_from_mach_thread_np(thread_port)))),
      libsystem_kernel_module_(GetLibSystemKernelModule(module_cache)) {
  GetSigtrampRange(&sigtramp_start_, &sigtramp_end_);
  // This class suspends threads, and those threads might be suspended in dyld.
  // Therefore, for all the system functions that might be linked in dynamically
  // that are used while threads are suspended, make calls to them to make sure
  // that they are linked up.
  x86_thread_state64_t thread_state;
  GetThreadState(thread_port_, &thread_state);
}

ThreadDelegateMac::~ThreadDelegateMac() = default;

std::unique_ptr<ThreadDelegate::ScopedSuspendThread>
ThreadDelegateMac::CreateScopedSuspendThread() {
  return std::make_unique<ScopedSuspendThread>(thread_port_);
}

// NO HEAP ALLOCATIONS.
bool ThreadDelegateMac::GetThreadContext(x86_thread_state64_t* thread_context) {
  return GetThreadState(thread_port_, thread_context);
}

// NO HEAP ALLOCATIONS.
uintptr_t ThreadDelegateMac::GetStackBaseAddress() const {
  return thread_stack_base_address_;
}

// NO HEAP ALLOCATIONS.
bool ThreadDelegateMac::CanCopyStack(uintptr_t stack_pointer) {
  return true;
}

std::vector<uintptr_t*> ThreadDelegateMac::GetRegistersToRewrite(
    x86_thread_state64_t* thread_context) {
  return {
      &AsUintPtr(&thread_context->__rbx), &AsUintPtr(&thread_context->__rbp),
      &AsUintPtr(&thread_context->__rsp), &AsUintPtr(&thread_context->__r12),
      &AsUintPtr(&thread_context->__r13), &AsUintPtr(&thread_context->__r14),
      &AsUintPtr(&thread_context->__r15)};
}

UnwindResult ThreadDelegateMac::WalkNativeFrames(
    x86_thread_state64_t* thread_context,
    uintptr_t stack_top,
    ModuleCache* module_cache,
    std::vector<ProfileBuilder::Frame>* stack) {
  // We expect the frame corresponding to the |thread_context| register state to
  // exist within |stack|.
  DCHECK_GT(stack->size(), 0u);

  // There isn't an official way to create a unw_context other than to create it
  // from the current state of the current thread's stack. Since we're walking a
  // different thread's stack we must forge a context. The unw_context is just a
  // copy of the 16 main registers followed by the instruction pointer, nothing
  // more.  Coincidentally, the first 17 items of the x86_thread_state64_t type
  // are exactly those registers in exactly the same order, so just bulk copy
  // them over.
  unw_context_t unwind_context;
  memcpy(&unwind_context, thread_context, sizeof(uintptr_t) * 17);

  // Avoid an out-of-bounds read bug in libunwind that can crash us in some
  // circumstances. If we're subject to that case, just record the first frame
  // and bail. See MayTriggerUnwInitLocalCrash for details.
  if (stack->back().module && MayTriggerUnwInitLocalCrash(stack->back().module))
    return UnwindResult::ABORTED;

  unw_cursor_t unwind_cursor;
  unw_init_local(&unwind_cursor, &unwind_context);

  int step_result;
  for (;;) {
    // First frame unwind step, check pre-conditions for attempting a frame
    // unwind.
    if (!stack->back().module) {
      // There's no loaded module containing the instruction pointer. This is
      // due to either executing code that is not in a module (e.g. V8
      // runtime-generated code), or to a previous bad unwind.
      //
      // The bad unwind scenario can occur in frameless (non-DWARF) unwinding,
      // which works by fetching the function's stack size from the unwind
      // encoding or stack, and adding it to the stack pointer to determine the
      // function's return address.
      //
      // If we're in a function prologue or epilogue, the actual stack size may
      // be smaller than it will be during the normal course of execution. When
      // libunwind adds the expected stack size, it will look for the return
      // address in the wrong place. This check ensures we don't continue trying
      // to unwind using the resulting bad IP value.
      //
      // We return UNRECOGNIZED_FRAME on the optimistic assumption that this may
      // be a frame the AuxUnwinder knows how to handle (e.g. a frame in V8
      // generated code).
      return UnwindResult::UNRECOGNIZED_FRAME;
    }

    // Don't continue if we're in sigtramp. Unwinding this from another thread
    // is very fragile. It's a complex DWARF unwind that needs to restore the
    // entire thread context which was saved by the kernel when the interrupt
    // occurred.
    if (stack->back().instruction_pointer >= sigtramp_start_ &&
        stack->back().instruction_pointer < sigtramp_end_) {
      return UnwindResult::ABORTED;
    }

    // Don't continue if rbp appears to be invalid (due to a previous bad
    // unwind).
    if (!HasValidRbp(&unwind_cursor, stack_top))
      return UnwindResult::ABORTED;

    // Second frame unwind step: do the unwind.
    unw_word_t prev_stack_pointer;
    unw_get_reg(&unwind_cursor, UNW_REG_SP, &prev_stack_pointer);
    step_result = unw_step(&unwind_cursor);

    if (step_result == 0 && stack->size() == 1u) {
      // libunwind is designed to be triggered by user code on their own thread,
      // if it hits a library that has no unwind info for the function that is
      // being executed, it just stops. This isn't a problem in the normal case,
      // but in the case where this is the first frame unwind, it's quite
      // possible that the stack being walked is stopped in a function that
      // bridges to the kernel and thus is missing the unwind info.

      // For now, just unwind the single case where the thread is stopped in a
      // function in libsystem_kernel.
      uint64_t& rsp = unwind_context.data[7];
      uint64_t& rip = unwind_context.data[16];
      if (module_cache->GetModuleForAddress(rip) == libsystem_kernel_module_) {
        rip = *reinterpret_cast<uint64_t*>(rsp);
        rsp += 8;
        // Reset the cursor.
        unw_init_local(&unwind_cursor, &unwind_context);
        // Mock a successful step_result.
        step_result = 1;
      }
    }

    // Third frame unwind step: check the result of the unwind.
    if (step_result < 0)
      return UnwindResult::ABORTED;

    // Fourth frame unwind step: record the frame to which we just unwound.
    unw_word_t instruction_pointer;
    unw_get_reg(&unwind_cursor, UNW_REG_IP, &instruction_pointer);
    unw_word_t stack_pointer;
    unw_get_reg(&unwind_cursor, UNW_REG_SP, &stack_pointer);

    // Record the frame if the last step was successful.
    if (step_result > 0 ||
        // libunwind considers the unwind complete and returns 0 if no unwind
        // info was found for the current instruction pointer. It performs this
        // check both before *and* after stepping the cursor. In the former case
        // no action is taken, but in the latter case an unwind was successfully
        // performed prior to the check. Distinguish these cases by checking
        // whether the stack pointer was moved by unw_step. If so, record the
        // new frame to enable non-native unwinders to continue the unwinding.
        (step_result == 0 && stack_pointer > prev_stack_pointer)) {
      stack->emplace_back(
          instruction_pointer,
          module_cache->GetModuleForAddress(instruction_pointer));
    }

    // libunwind returns 0 if it can't continue because no unwind info was found
    // for the current instruction pointer. This could be due to unwinding past
    // the entry point, in which case the unwind would be complete. It could
    // also be due to unwinding to a function that simply doesn't have unwind
    // info, in which case the unwind should be aborted. Or it could be due to
    // unwinding to code not in a module, in which case the unwind might be
    // continuable by a non-native unwinder. We don't have a good way to
    // distinguish these cases, so return UNRECOGNIZED_FRAME to at least
    // signify that we couldn't unwind further.
    if (step_result == 0)
      return UnwindResult::UNRECOGNIZED_FRAME;
  }

  NOTREACHED();
  return UnwindResult::COMPLETED;
}

}  // namespace base
