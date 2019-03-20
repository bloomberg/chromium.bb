// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/native_stack_sampler.h"

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
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/strings/string_number_conversions.h"

extern "C" {
void _sigtramp(int, int, struct sigset*);
}

namespace base {

using Frame = StackSamplingProfiler::Frame;
using ProfileBuilder = StackSamplingProfiler::ProfileBuilder;

namespace {

// Stack walking --------------------------------------------------------------

// Fills |state| with |target_thread|'s context.
//
// Note that this is called while a thread is suspended. Make very very sure
// that no shared resources (e.g. memory allocators) are used for the duration
// of this function.
bool GetThreadState(thread_act_t target_thread, x86_thread_state64_t* state) {
  auto count = static_cast<mach_msg_type_number_t>(x86_THREAD_STATE64_COUNT);
  return thread_get_state(target_thread, x86_THREAD_STATE64,
                          reinterpret_cast<thread_state_t>(state),
                          &count) == KERN_SUCCESS;
}

// If the value at |pointer| points to the original stack, rewrites it to point
// to the corresponding location in the copied stack.
//
// Note that this is called while a thread is suspended. Make very very sure
// that no shared resources (e.g. memory allocators) are used for the duration
// of this function.
uintptr_t RewritePointerIfInOriginalStack(
    const uintptr_t* original_stack_bottom,
    const uintptr_t* original_stack_top,
    uintptr_t* stack_copy_bottom,
    uintptr_t pointer) {
  auto original_stack_bottom_int =
      reinterpret_cast<uintptr_t>(original_stack_bottom);
  auto original_stack_top_int = reinterpret_cast<uintptr_t>(original_stack_top);
  auto stack_copy_bottom_int = reinterpret_cast<uintptr_t>(stack_copy_bottom);

  if (pointer < original_stack_bottom_int || pointer >= original_stack_top_int)
    return pointer;

  return stack_copy_bottom_int + (pointer - original_stack_bottom_int);
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
// sizeof(void*) aligned.
//
// Note that this is called while a thread is suspended. Make very very sure
// that no shared resources (e.g. memory allocators) are used for the duration
// of this function.
void CopyStackAndRewritePointers(uintptr_t* stack_copy_bottom,
                                 const uintptr_t* original_stack_bottom,
                                 const uintptr_t* original_stack_top,
                                 x86_thread_state64_t* thread_state)
    NO_SANITIZE("address") {
  size_t count = original_stack_top - original_stack_bottom;
  for (size_t pos = 0; pos < count; ++pos) {
    stack_copy_bottom[pos] = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom,
        original_stack_bottom[pos]);
  }

  uint64_t* rewrite_registers[] = {&thread_state->__rbx, &thread_state->__rbp,
                                   &thread_state->__rsp, &thread_state->__r12,
                                   &thread_state->__r13, &thread_state->__r14,
                                   &thread_state->__r15};
  for (auto* reg : rewrite_registers) {
    *reg = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom, *reg);
  }
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

// ScopedSuspendThread --------------------------------------------------------

// Suspends a thread for the lifetime of the object.
class ScopedSuspendThread {
 public:
  explicit ScopedSuspendThread(mach_port_t thread_port)
      : thread_port_(thread_suspend(thread_port) == KERN_SUCCESS
                         ? thread_port
                         : MACH_PORT_NULL) {}

  ~ScopedSuspendThread() {
    if (!was_successful())
      return;

    kern_return_t kr = thread_resume(thread_port_);
    MACH_CHECK(kr == KERN_SUCCESS, kr) << "thread_resume";
  }

  bool was_successful() const { return thread_port_ != MACH_PORT_NULL; }

 private:
  mach_port_t thread_port_;

  DISALLOW_COPY_AND_ASSIGN(ScopedSuspendThread);
};

}  // namespace

// NativeStackSamplerMac ------------------------------------------------------

class NativeStackSamplerMac : public NativeStackSampler {
 public:
  NativeStackSamplerMac(mach_port_t thread_port,
                        ModuleCache* module_cache,
                        NativeStackSamplerTestDelegate* test_delegate);
  ~NativeStackSamplerMac() override;

  // StackSamplingProfiler::NativeStackSampler:
  void RecordStackFrames(StackBuffer* stack_buffer,
                         ProfileBuilder* profile_builder) override;

 private:
  // Suspends the thread with |thread_handle|, copies its stack, register
  // context, and current metadata and resumes the thread. Returns true on
  // success.
  static bool CopyStack(mach_port_t thread_port,
                        const void* base_address,
                        StackBuffer* stack_buffer,
                        ProfileBuilder* profile_builder,
                        x86_thread_state64_t* thread_state,
                        uintptr_t* stack_top);

  // Walks the stack represented by |thread_state|, calling back to the
  // provided lambda for each frame.
  std::vector<Frame> WalkStack(const x86_thread_state64_t& thread_state,
                               uintptr_t stack_top);

  // Weak reference: Mach port for thread being profiled.
  mach_port_t thread_port_;

  // Maps a module's address range to the module.
  ModuleCache* const module_cache_;

  NativeStackSamplerTestDelegate* const test_delegate_;

  // The stack base address corresponding to |thread_handle_|.
  const void* const thread_stack_base_address_;

  // Cached pointer to the libsystem_kernel module.
  const ModuleCache::Module* const libsystem_kernel_module_;

  // The address range of |_sigtramp|, the signal trampoline function.
  uintptr_t sigtramp_start_;
  uintptr_t sigtramp_end_;

  DISALLOW_COPY_AND_ASSIGN(NativeStackSamplerMac);
};

NativeStackSamplerMac::NativeStackSamplerMac(
    mach_port_t thread_port,
    ModuleCache* module_cache,
    NativeStackSamplerTestDelegate* test_delegate)
    : thread_port_(thread_port),
      module_cache_(module_cache),
      test_delegate_(test_delegate),
      thread_stack_base_address_(
          pthread_get_stackaddr_np(pthread_from_mach_thread_np(thread_port))),
      libsystem_kernel_module_(GetLibSystemKernelModule(module_cache)) {
  GetSigtrampRange(&sigtramp_start_, &sigtramp_end_);
  // This class suspends threads, and those threads might be suspended in dyld.
  // Therefore, for all the system functions that might be linked in dynamically
  // that are used while threads are suspended, make calls to them to make sure
  // that they are linked up.
  x86_thread_state64_t thread_state;
  GetThreadState(thread_port_, &thread_state);
}

NativeStackSamplerMac::~NativeStackSamplerMac() {}

void NativeStackSamplerMac::RecordStackFrames(StackBuffer* stack_buffer,
                                              ProfileBuilder* profile_builder) {
  x86_thread_state64_t thread_state;
  uintptr_t stack_top;

  bool success =
      CopyStack(thread_port_, thread_stack_base_address_, stack_buffer,
                profile_builder, &thread_state, &stack_top);
  if (!success)
    return;

  if (test_delegate_)
    test_delegate_->OnPreStackWalk();

  // Walk the stack and record it.
  profile_builder->OnSampleCompleted(WalkStack(thread_state, stack_top));
}

// static
bool NativeStackSamplerMac::CopyStack(mach_port_t thread_port,
                                      const void* base_address,
                                      StackBuffer* stack_buffer,
                                      ProfileBuilder* profile_builder,
                                      x86_thread_state64_t* thread_state,
                                      uintptr_t* stack_top) {
  // IMPORTANT NOTE: Do not do ANYTHING in this in this scope that might
  // allocate memory, including indirectly via use of DCHECK/CHECK or other
  // logging statements. Otherwise this code can deadlock on heap locks acquired
  // by the target thread before it was suspended.

  ScopedSuspendThread suspend_thread(thread_port);
  if (!suspend_thread.was_successful())
    return false;

  if (!GetThreadState(thread_port, thread_state))
    return false;

  auto top = reinterpret_cast<uintptr_t>(base_address);
  uintptr_t bottom = thread_state->__rsp;
  if (bottom >= top)
    return false;

  uintptr_t stack_size = top - bottom;
  if (stack_size > stack_buffer->size())
    return false;

  profile_builder->RecordMetadata();

  CopyStackAndRewritePointers(
      reinterpret_cast<uintptr_t*>(stack_buffer->buffer()),
      reinterpret_cast<uintptr_t*>(bottom), reinterpret_cast<uintptr_t*>(top),
      thread_state);

  *stack_top = reinterpret_cast<uintptr_t>(stack_buffer->buffer()) + stack_size;

  return true;
}

std::vector<Frame> NativeStackSamplerMac::WalkStack(
    const x86_thread_state64_t& thread_state,
    uintptr_t stack_top) {
  std::vector<Frame> stack;

  // Reserve enough memory for most stacks, to avoid repeated
  // allocations. Approximately 99.9% of recorded stacks are 128 frames or
  // fewer.
  stack.reserve(128);

  // There isn't an official way to create a unw_context other than to create it
  // from the current state of the current thread's stack. Since we're walking a
  // different thread's stack we must forge a context. The unw_context is just a
  // copy of the 16 main registers followed by the instruction pointer, nothing
  // more.  Coincidentally, the first 17 items of the x86_thread_state64_t type
  // are exactly those registers in exactly the same order, so just bulk copy
  // them over.
  unw_context_t unwind_context;
  memcpy(&unwind_context, &thread_state, sizeof(uintptr_t) * 17);

  // Avoid an out-of-bounds read bug in libunwind that can crash us in some
  // circumstances. If we're subject to that case, just record the first frame
  // and bail. See MayTriggerUnwInitLocalCrash for details.
  const ModuleCache::Module* leaf_frame_module =
      module_cache_->GetModuleForAddress(thread_state.__rip);
  if (leaf_frame_module && MayTriggerUnwInitLocalCrash(leaf_frame_module)) {
    return {Frame(thread_state.__rip, leaf_frame_module)};
  }

  unw_cursor_t unwind_cursor;
  unw_init_local(&unwind_cursor, &unwind_context);

  bool at_top_frame = true;
  int step_result;
  do {
    unw_word_t instruction_pointer;
    unw_get_reg(&unwind_cursor, UNW_REG_IP, &instruction_pointer);

    // Ensure IP is in a module.
    //
    // Frameless unwinding (non-DWARF) works by fetching the function's stack
    // size from the unwind encoding or stack, and adding it to the stack
    // pointer to determine the function's return address.
    //
    // If we're in a function prologue or epilogue, the actual stack size may be
    // smaller than it will be during the normal course of execution. When
    // libunwind adds the expected stack size, it will look for the return
    // address in the wrong place. This check should ensure that we bail before
    // trying to deref a bad IP obtained this way in the previous frame.
    const ModuleCache::Module* module =
        module_cache_->GetModuleForAddress(instruction_pointer);
    if (!module)
      break;

    // Record the frame.
    stack.emplace_back(instruction_pointer, module);

    // Don't continue if we're in sigtramp. Unwinding this from another thread
    // is very fragile. It's a complex DWARF unwind that needs to restore the
    // entire thread context which was saved by the kernel when the interrupt
    // occurred.
    if (instruction_pointer >= sigtramp_start_ &&
        instruction_pointer < sigtramp_end_)
      break;

    // Don't continue if rbp appears to be invalid (due to a previous bad
    // unwind).
    if (!HasValidRbp(&unwind_cursor, stack_top))
      break;

    step_result = unw_step(&unwind_cursor);

    if (step_result == 0 && at_top_frame) {
      // libunwind is designed to be triggered by user code on their own thread,
      // if it hits a library that has no unwind info for the function that is
      // being executed, it just stops. This isn't a problem in the normal case,
      // but in this case, it's quite possible that the stack being walked is
      // stopped in a function that bridges to the kernel and thus is missing
      // the unwind info.

      // For now, just unwind the single case where the thread is stopped in a
      // function in libsystem_kernel.
      uint64_t& rsp = unwind_context.data[7];
      uint64_t& rip = unwind_context.data[16];
      if (module_cache_->GetModuleForAddress(rip) == libsystem_kernel_module_) {
        rip = *reinterpret_cast<uint64_t*>(rsp);
        rsp += 8;
        // Reset the cursor.
        unw_init_local(&unwind_cursor, &unwind_context);
        // Mock a successful step_result.
        step_result = 1;
      }
    }

    at_top_frame = false;
  } while (step_result > 0);

  return stack;
}

// NativeStackSampler ---------------------------------------------------------

// static
std::unique_ptr<NativeStackSampler> NativeStackSampler::Create(
    PlatformThreadId thread_id,
    ModuleCache* module_cache,
    NativeStackSamplerTestDelegate* test_delegate) {
  return std::make_unique<NativeStackSamplerMac>(thread_id, module_cache,
                                                 test_delegate);
}

// static
size_t NativeStackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default macOS main thread stack size
  // of 8 MB (DFLSSIZ in <i386/vmparam.h>) with extra wiggle room.
  return stack_size > 0 ? stack_size : 12 * 1024 * 1024;
}

}  // namespace base
