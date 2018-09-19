// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/stack_unwinder_android.h"

#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syscall.h>
#include "link.h"

#include <algorithm>
#include <memory>

#include "base/debug/proc_maps_linux.h"
#include "base/logging.h"
#include "base/trace_event/cfi_backtrace_android.h"
#include "libunwind.h"

using base::trace_event::CFIBacktraceAndroid;
using base::debug::MappedMemoryRegion;

namespace {

// Waitable event implementation with futex and without DCHECK(s), since signal
// handlers cannot allocate memory or use pthread api.
class AsyncSafeWaitableEvent {
 public:
  AsyncSafeWaitableEvent() { base::subtle::Release_Store(&futex_, 0); }
  ~AsyncSafeWaitableEvent() {}

  bool Wait() {
    // futex() can wake up spuriously if this memory address was previously used
    // for a pthread mutex. So, also check the condition.
    while (true) {
      int res = syscall(SYS_futex, &futex_, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, 0,
                        nullptr, nullptr, 0);
      if (base::subtle::Acquire_Load(&futex_) != 0)
        return true;
      if (res != 0)
        return false;
    }
  }

  void Signal() {
    base::subtle::Release_Store(&futex_, 1);
    syscall(SYS_futex, &futex_, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, nullptr,
            nullptr, 0);
  }

 private:
  int futex_;
};

// Scoped signal event that calls Signal on the AsyncSafeWaitableEvent at
// destructor.
class ScopedEventSignaller {
 public:
  ScopedEventSignaller(AsyncSafeWaitableEvent* event) : event_(event) {}
  ~ScopedEventSignaller() { event_->Signal(); }

 private:
  AsyncSafeWaitableEvent* event_;
};

// Unwinds from given |cursor| readable by libunwind, and returns
// the number of frames added to the output. This function can unwind through
// android framework and then chrome functions. It cannot handle the cases when
// the chrome functions are called by android framework again, since we cannot
// create the right context for libunwind from chrome functions.
// TODO(ssid): This function should support unwinding from chrome to android
// libraries also.
size_t TraceStackWithContext(unw_cursor_t* cursor,
                             CFIBacktraceAndroid* cfi_unwinder,
                             const tracing::StackUnwinderAndroid* unwinder,
                             const void** out_trace,
                             size_t max_depth) {
  size_t depth = 0;
  unw_word_t ip = 0, sp = 0;
  do {
    unw_get_reg(cursor, UNW_REG_IP, &ip);
    unw_get_reg(cursor, UNW_REG_SP, &sp);

    // If address is in chrome library, then use CFI unwinder since chrome might
    // not have EHABI unwind tables.
    if (CFIBacktraceAndroid::is_chrome_address(ip))
      break;

    // Break if pc is not from any mapped region. Something went wrong while
    // unwinding.
    if (!unwinder->IsAddressMapped(ip))
      break;

    // If it is chrome address, the cfi unwinder will include it.
    out_trace[depth++] = reinterpret_cast<void*>(ip);
  } while (unw_step(cursor) && depth < max_depth - 1);

  if (CFIBacktraceAndroid::is_chrome_address(ip)) {
    // Continue unwinding CFI unwinder if we found stack frame from chrome
    // library.
    return depth +
           cfi_unwinder->Unwind(ip, sp, out_trace + depth, max_depth - depth);
  }
  return depth;
}

// Returns the offset of stack pointer for the given program counter in chrome
// library.
bool GetCFIForPC(CFIBacktraceAndroid* cfi_unwinder,
                 uintptr_t pc,
                 CFIBacktraceAndroid::CFIRow* cfi) {
  return cfi_unwinder->FindCFIRowForPC(
      pc - CFIBacktraceAndroid::executable_start_addr(), cfi);
}

constexpr size_t kMaxStackBytesCopied = 1024 * 1024;

// Struct to store the arguments to the signal handler.
struct HandlerParams {
  const tracing::StackUnwinderAndroid* unwinder;
  // The event is signalled when signal handler is done executing.
  AsyncSafeWaitableEvent* event;

  // Return values:
  // Successfully copied the stack segment.
  bool* success;
  // The register context of the thread used by libunwind.
  unw_context_t* context;
  // The value of Stack pointer of the thread.
  uintptr_t* sp;
  // The address where the full stack is copied to.
  char* stack_copy_buffer;
};

// Argument passed to the ThreadSignalHandler() from the sampling thread to the
// sampled (stopped) thread. This value is set just before sending kill signal
// to the thread and reset when handler is done.
base::subtle::AtomicWord g_handler_params;

// The signal handler is called on the stopped thread as an additional stack
// frame. This relies on no alternate sigaltstack() being set. This function
// skips the handler frame on stack and unwinds the rest of the stack frames.
// This function should use async-safe functions only. The only call that could
// allocate memory on heap would be the cache in cfi unwinder. We need to ensure
// that AllocateCacheForCurrentThread() is called on the stopped thread before
// trying to get stack trace from the thread. See
// https://www.gnu.org/software/libc/manual/html_node/Nonreentrancy.html#Nonreentrancy.
static void ThreadSignalHandler(int n, siginfo_t* siginfo, void* sigcontext) {
  HandlerParams* params = reinterpret_cast<HandlerParams*>(
      base::subtle::Acquire_Load(&g_handler_params));
  ScopedEventSignaller e(params->event);
  *params->success = false;

  uintptr_t sp = 0;
  if (unw_getcontext(params->context) != 0)
    return;

  asm volatile("mov %0, sp" : "=r"(sp));
  *params->sp = sp;

  uintptr_t stack_base_addr = params->unwinder->GetEndAddressOfRegion(sp);
  size_t size = stack_base_addr - sp;
  if (stack_base_addr == 0 || size > kMaxStackBytesCopied)
    return;
  memcpy(params->stack_copy_buffer, reinterpret_cast<void*>(sp), size);
  *params->success = true;
}

// ARM EXIDX table contains addresses in sorted order with unwind data, each of
// 32 bits.
struct FakeExidx {
  uintptr_t pc;
  uintptr_t index_data;
};

}  // namespace

extern "C" {

_Unwind_Ptr __real_dl_unwind_find_exidx(_Unwind_Ptr, int*);

// Override the default |dl_unwind_find_exidx| function used by libunwind to
// give a fake unwind table just for the handler function. Otherwise call the
// original function. Libunwind marks the cursor invalid if it finds even one
// frame without unwind info. Mocking the info keeps the unwind cursor valid
// after unwind_init_local() within ThreadSignalHandler().
__attribute__((visibility("default"), noinline)) _Unwind_Ptr
__wrap_dl_unwind_find_exidx(_Unwind_Ptr pc, int* length) {
  if (!CFIBacktraceAndroid::is_chrome_address(pc)) {
    return __real_dl_unwind_find_exidx(pc, length);
  }
  // Fake exidx table that is passed to libunwind to work with chrome functions.
  // 0x80000000 has high bit set to 1. This means the unwind data is inline and
  // not in exception table (section 5 EHABI). 0 on the second high byte causes
  // a 0 proceedure to be lsda. But this is never executed since the pc and sp
  // will be overridden, before calling unw_step.
  static const FakeExidx chrome_exidx_data[] = {
      {CFIBacktraceAndroid::executable_start_addr(), 0x80000000},
      {CFIBacktraceAndroid::executable_end_addr(), 0x80000000}};
  *length = sizeof(chrome_exidx_data);
  return reinterpret_cast<_Unwind_Ptr>(chrome_exidx_data);
}

}  // extern "C"

namespace tracing {

StackUnwinderAndroid::StackUnwinderAndroid() {}
StackUnwinderAndroid::~StackUnwinderAndroid() {}

void StackUnwinderAndroid::Initialize() {
  is_initialized_ = true;

  // Ensure Chrome unwinder is initialized.
  CFIBacktraceAndroid::GetInitializedInstance();

  // Parses /proc/self/maps.
  std::string contents;
  if (!base::debug::ReadProcMaps(&contents)) {
    NOTREACHED();
  }
  if (!base::debug::ParseProcMaps(contents, &regions_)) {
    NOTREACHED();
  }
  std::sort(regions_.begin(), regions_.end(),
            [](const MappedMemoryRegion& a, const MappedMemoryRegion& b) {
              return a.start < b.start;
            });
}

size_t StackUnwinderAndroid::TraceStack(const void** out_trace,
                                        size_t max_depth) const {
  DCHECK(is_initialized_);
  unw_cursor_t cursor;
  unw_context_t context;

  if (unw_getcontext(&context) != 0)
    return 0;
  if (unw_init_local(&cursor, &context) != 0)
    return 0;
  return TraceStackWithContext(&cursor,
                               CFIBacktraceAndroid::GetInitializedInstance(),
                               this, out_trace, max_depth);
}

size_t StackUnwinderAndroid::TraceStack(base::PlatformThreadId tid,
                                        const void** out_trace,
                                        size_t max_depth) const {
  // Stops the thread with given tid with a signal handler. The signal handler
  // copies the stack of the thread and returns. This function tries to unwind
  // stack frames from the copied stack.
  DCHECK(is_initialized_);
  AsyncSafeWaitableEvent wait_event;
  std::unique_ptr<char[]> stack_copy_buffer(new char[kMaxStackBytesCopied]);
  bool copied = false;
  unw_context_t context;
  uintptr_t sp = 0;
  HandlerParams params = {this,     &wait_event, &copied,
                          &context, &sp,         stack_copy_buffer.get()};
  base::subtle::Release_Store(&g_handler_params,
                              reinterpret_cast<uintptr_t>(&params));

  // Change the signal handler for the thread to unwind function, which should
  // execute on the stack so that we will be able to unwind.
  struct sigaction act;
  struct sigaction oact;
  memset(&act, 0, sizeof(act));
  act.sa_sigaction = ThreadSignalHandler;
  // TODO(ssid): SA_ONSTACK will not work if sigaltstack was set. So, unwind
  // should work without this flag.
  act.sa_flags = SA_RESTART | SA_SIGINFO | SA_ONSTACK;
  sigemptyset(&act.sa_mask);
  // SIGURG is chosen here because we observe no crashes with this signal and
  // neither Chrome or the AOSP sets up a special handler for this signal.
  if (!sigaction(SIGURG, &act, &oact)) {
    kill(tid, SIGURG);
    bool finished_waiting = wait_event.Wait();

    bool changed = sigaction(SIGURG, &oact, &act) == 0;
    DCHECK(changed);
    if (!finished_waiting) {
      NOTREACHED();
      return 0;
    }
  }
  base::subtle::Release_Store(&g_handler_params, 0);
  if (!copied)
    return 0;

  // Context contains list of saved registers. Replace the SP to the copied
  // stack. The SP should be one of the first 16 registers.
  bool replaced_sp = false;
  for (size_t i = 0; i < 17; ++i) {
    uintptr_t* data = reinterpret_cast<uintptr_t*>(&context) + i;
    if (*data == sp) {
      *data = reinterpret_cast<uintptr_t>(stack_copy_buffer.get());
      replaced_sp = true;
      break;
    }
  }
  DCHECK(replaced_sp);

  // Initialize an unwind cursor on copied stack.
  unw_cursor_t cursor;
  if (unw_init_local(&cursor, &context) != 0)
    return 0;
  uintptr_t ip = 0;
  unw_get_reg(&cursor, UNW_REG_SP, &sp);
  DCHECK_EQ(sp, reinterpret_cast<uintptr_t>(stack_copy_buffer.get()));
  unw_get_reg(&cursor, UNW_REG_IP, &ip);

  // Unwind handler function (ThreadSignalHandler()) since libunwind cannot
  // handle chrome functions. Then call either libunwind or use chrome's
  // unwinder based on the next function in the stack.
  auto* cfi_unwinder = CFIBacktraceAndroid::GetInitializedInstance();
  static CFIBacktraceAndroid::CFIRow cfi;
  static bool found = GetCFIForPC(cfi_unwinder, ip, &cfi);
  if (!found)
    return 0;
  sp = sp + cfi.cfa_offset;
  memcpy(&ip, reinterpret_cast<uintptr_t*>(sp - cfi.ra_offset),
         sizeof(uintptr_t));

  // Do not use libunwind if we stopped at chrome frame.
  if (CFIBacktraceAndroid::is_chrome_address(ip))
    return cfi_unwinder->Unwind(ip, sp, out_trace, max_depth);

  // Reset the unwind cursor to previous function and continue with libunwind.
  // TODO(ssid): Dynamic allocation functions might require registers to be
  // restored.
  unw_set_reg(&cursor, UNW_REG_SP, sp);
  unw_set_reg(&cursor, UNW_REG_IP, ip);

  return TraceStackWithContext(&cursor, cfi_unwinder, this, out_trace,
                               max_depth);
}

uintptr_t StackUnwinderAndroid::GetEndAddressOfRegion(uintptr_t addr) const {
  auto it =
      std::lower_bound(regions_.begin(), regions_.end(), addr,
                       [](const MappedMemoryRegion& region, uintptr_t addr) {
                         return region.start < addr;
                       });
  if (it == regions_.begin())
    return 0;
  --it;
  if (it->start <= addr && it->end > addr)
    return it->end;
  return 0;
}

bool StackUnwinderAndroid::IsAddressMapped(uintptr_t pc) const {
  // TODO(ssid): We only need to check regions which are file mapped.
  return GetEndAddressOfRegion(pc) != 0;
}

}  // namespace tracing
