// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/thread_delegate.h"
#include "base/profiler/unwinder.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

namespace {

// If the value at |pointer| points to the original stack, rewrite it to point
// to the corresponding location in the copied stack. NO HEAP ALLOCATIONS.
// static
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
// NO HEAP ALLOCATIONS.
//
// static
NO_SANITIZE("address")
void CopyStackContentsAndRewritePointers(const uintptr_t* original_stack_bottom,
                                         const uintptr_t* original_stack_top,
                                         uintptr_t* stack_copy_bottom) {
  const uintptr_t* src = original_stack_bottom;
  uintptr_t* dst = stack_copy_bottom;
  for (; src < original_stack_top; ++src, ++dst) {
    *dst = RewritePointerIfInOriginalStack(
        original_stack_bottom, original_stack_top, stack_copy_bottom, *src);
  }
}

}  // namespace

StackSamplerImpl::StackSamplerImpl(
    std::unique_ptr<ThreadDelegate> thread_delegate,
    std::unique_ptr<Unwinder> native_unwinder,
    ModuleCache* module_cache,
    StackSamplerTestDelegate* test_delegate)
    : thread_delegate_(std::move(thread_delegate)),
      native_unwinder_(std::move(native_unwinder)),
      module_cache_(module_cache),
      test_delegate_(test_delegate) {}

StackSamplerImpl::~StackSamplerImpl() = default;

void StackSamplerImpl::AddAuxUnwinder(Unwinder* unwinder) {
  aux_unwinder_ = unwinder;
}

void StackSamplerImpl::RecordStackFrames(StackBuffer* stack_buffer,
                                         ProfileBuilder* profile_builder) {
  DCHECK(stack_buffer);

  RegisterContext thread_context;
  uintptr_t stack_top;
  bool success =
      CopyStack(stack_buffer, &stack_top, profile_builder, &thread_context);
  if (!success)
    return;

  if (test_delegate_)
    test_delegate_->OnPreStackWalk();

  profile_builder->OnSampleCompleted(WalkStack(&thread_context, stack_top));
}

// Suspends the thread, copies its stack, top address of the stack copy, and
// register context, records the current metadata, then resumes the thread.
// Returns true on success, and returns the copied state via the params. NO HEAP
// ALLOCATIONS within the ScopedSuspendThread scope.
bool StackSamplerImpl::CopyStack(StackBuffer* stack_buffer,
                                 uintptr_t* stack_top,
                                 ProfileBuilder* profile_builder,
                                 RegisterContext* thread_context) {
  const uintptr_t top = thread_delegate_->GetStackBaseAddress();
  uintptr_t bottom = 0;
  {
    // Allocation of the ScopedSuspendThread object itself is OK since it
    // necessarily occurs before the thread is suspended by the object.
    std::unique_ptr<ThreadDelegate::ScopedSuspendThread> suspend_thread =
        thread_delegate_->CreateScopedSuspendThread();

    if (!suspend_thread->WasSuccessful())
      return false;

    if (!thread_delegate_->GetThreadContext(thread_context))
      return false;

    bottom = RegisterContextStackPointer(thread_context);

    // The StackBuffer allocation is expected to be at least as large as the
    // largest stack region allocation on the platform, but check just in case
    // it isn't *and* the actual stack itself exceeds the buffer allocation
    // size.
    if ((top - bottom) > stack_buffer->size())
      return false;

    if (!thread_delegate_->CanCopyStack(bottom))
      return false;

    profile_builder->RecordMetadata();

    CopyStackContentsAndRewritePointers(reinterpret_cast<uintptr_t*>(bottom),
                                        reinterpret_cast<uintptr_t*>(top),
                                        stack_buffer->buffer());
  }

  *stack_top =
      reinterpret_cast<uintptr_t>(stack_buffer->buffer()) + (top - bottom);

  for (uintptr_t* reg :
       thread_delegate_->GetRegistersToRewrite(thread_context)) {
    *reg = RewritePointerIfInOriginalStack(reinterpret_cast<uintptr_t*>(bottom),
                                           reinterpret_cast<uintptr_t*>(top),
                                           stack_buffer->buffer(), *reg);
  }

  return true;
}

// Walks the stack represented by |thread_context|, recording and returning the
// frames.
std::vector<Frame> StackSamplerImpl::WalkStack(RegisterContext* thread_context,
                                               uintptr_t stack_top) {
  std::vector<Frame> stack;
  // Reserve enough memory for most stacks, to avoid repeated
  // allocations. Approximately 99.9% of recorded stacks are 128 frames or
  // fewer.
  stack.reserve(128);

  // Record the first frame from the context values.
  stack.emplace_back(RegisterContextInstructionPointer(thread_context),
                     module_cache_->GetModuleForAddress(
                         RegisterContextInstructionPointer(thread_context)));

  native_unwinder_->TryUnwind(thread_context, stack_top, module_cache_, &stack);

  return stack;
}

}  // namespace base
