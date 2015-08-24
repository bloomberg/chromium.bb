// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/profiler/win32_stack_frame_unwinder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestUnwindFunctions : public Win32StackFrameUnwinder::UnwindFunctions {
 public:
  TestUnwindFunctions();

  PRUNTIME_FUNCTION LookupFunctionEntry(DWORD64 program_counter,
                                        PDWORD64 image_base) override;
  void VirtualUnwind(DWORD64 image_base,
                     DWORD64 program_counter,
                     PRUNTIME_FUNCTION runtime_function,
                     CONTEXT* context) override;

  void SetNoUnwindInfoForNextFrame();
  void SetImageBaseForNextFrame(DWORD64 image_base);

 private:
  enum { kRuntimeFunctionPointerIncrement = 1, kImageBaseIncrement = 1 << 20 };

  static const PRUNTIME_FUNCTION kNonNullRuntimeFunctionPointer;

  DWORD64 supplied_program_counter_;
  DWORD64 custom_image_base_;
  DWORD64 next_image_base_;
  bool next_lookup_returns_null_;

  DISALLOW_COPY_AND_ASSIGN(TestUnwindFunctions);
};

// This value is opaque to Win32StackFrameUnwinder.
const PRUNTIME_FUNCTION TestUnwindFunctions::kNonNullRuntimeFunctionPointer =
    reinterpret_cast<PRUNTIME_FUNCTION>(8);

TestUnwindFunctions::TestUnwindFunctions()
    : supplied_program_counter_(0),
      custom_image_base_(0),
      next_image_base_(kImageBaseIncrement),
      next_lookup_returns_null_(false) {
}

PRUNTIME_FUNCTION TestUnwindFunctions::LookupFunctionEntry(
    DWORD64 program_counter,
    PDWORD64 image_base) {
  supplied_program_counter_ = program_counter;
  if (custom_image_base_) {
    *image_base = custom_image_base_;
    custom_image_base_ = 0;
  } else {
    *image_base = next_image_base_;
    next_image_base_ += kImageBaseIncrement;
  }
  if (next_lookup_returns_null_) {
    next_lookup_returns_null_ = false;
    return nullptr;
  }

  return kNonNullRuntimeFunctionPointer;
}

void TestUnwindFunctions::VirtualUnwind(DWORD64 image_base,
                                        DWORD64 program_counter,
                                        PRUNTIME_FUNCTION runtime_function,
                                        CONTEXT* context) {
  EXPECT_EQ(next_image_base_ - kImageBaseIncrement, image_base);
  EXPECT_EQ(supplied_program_counter_, program_counter);
  // This function should only be called when LookupFunctionEntry returns a
  // non-null value.
  EXPECT_EQ(kNonNullRuntimeFunctionPointer, runtime_function);
}

void TestUnwindFunctions::SetNoUnwindInfoForNextFrame() {
  next_lookup_returns_null_ = true;
}

void TestUnwindFunctions::SetImageBaseForNextFrame(DWORD64 image_base) {
  next_image_base_ = image_base;
}

}  // namespace

class Win32StackFrameUnwinderTest : public testing::Test {
 protected:
  Win32StackFrameUnwinderTest() {}

  // This exists so that Win32StackFrameUnwinder's constructor can be private
  // with a single friend declaration of this test fixture.
  scoped_ptr<Win32StackFrameUnwinder> CreateUnwinder();

  TestUnwindFunctions unwind_functions_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Win32StackFrameUnwinderTest);
};

scoped_ptr<Win32StackFrameUnwinder>
Win32StackFrameUnwinderTest::CreateUnwinder() {
  return make_scoped_ptr(new Win32StackFrameUnwinder(&unwind_functions_));
}

// Checks the case where all frames have unwind information.
TEST_F(Win32StackFrameUnwinderTest, FramesWithUnwindInfo) {
  scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
  CONTEXT context = {0};
  EXPECT_TRUE(unwinder->TryUnwind(&context));
  EXPECT_TRUE(unwinder->TryUnwind(&context));
  EXPECT_TRUE(unwinder->TryUnwind(&context));
}

// Checks that the CONTEXT's stack pointer gets popped when the top frame has no
// unwind information.
TEST_F(Win32StackFrameUnwinderTest, FrameAtTopWithoutUnwindInfo) {
  scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
  CONTEXT context = {0};
  const DWORD64 original_rsp = 128;
  context.Rsp = original_rsp;
  unwind_functions_.SetNoUnwindInfoForNextFrame();
  EXPECT_TRUE(unwinder->TryUnwind(&context));
  EXPECT_EQ(original_rsp, context.Rip);
  EXPECT_EQ(original_rsp + 8, context.Rsp);

  EXPECT_TRUE(unwinder->TryUnwind(&context));
  EXPECT_TRUE(unwinder->TryUnwind(&context));
}

// Checks that a frame below the top of the stack with missing unwind info
// results in blacklisting the module.
TEST_F(Win32StackFrameUnwinderTest, BlacklistedModule) {
  const DWORD64 image_base_for_module_with_bad_function = 1024;
  {
    // First stack, with a bad function below the top of the stack.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    EXPECT_TRUE(unwinder->TryUnwind(&context));

    unwind_functions_.SetNoUnwindInfoForNextFrame();
    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_module_with_bad_function);
    EXPECT_FALSE(unwinder->TryUnwind(&context));
  }

  {
    // Second stack; check that a function at the top of the stack without
    // unwind info from the previously-seen module is blacklisted.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    unwind_functions_.SetNoUnwindInfoForNextFrame();
    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_module_with_bad_function);
    EXPECT_FALSE(unwinder->TryUnwind(&context));
  }

  {
    // Third stack; check that a function at the top of the stack *with* unwind
    // info from the previously-seen module is not blacklisted. Then check that
    // functions below the top of the stack with unwind info are not
    // blacklisted, regardless of whether they are in the previously-seen
    // module.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_module_with_bad_function);
    EXPECT_TRUE(unwinder->TryUnwind(&context));

    EXPECT_TRUE(unwinder->TryUnwind(&context));

    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_module_with_bad_function);
    EXPECT_TRUE(unwinder->TryUnwind(&context));
  }

  {
    // Fourth stack; check that a function at the top of the stack without
    // unwind info and not from the previously-seen module is not
    // blacklisted. Then check that functions below the top of the stack with
    // unwind info are not blacklisted, regardless of whether they are in the
    // previously-seen module.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    unwind_functions_.SetNoUnwindInfoForNextFrame();
    EXPECT_TRUE(unwinder->TryUnwind(&context));

    EXPECT_TRUE(unwinder->TryUnwind(&context));

    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_module_with_bad_function);
    EXPECT_TRUE(unwinder->TryUnwind(&context));
  }
}

// Checks that a frame below the top of the stack with missing unwind info does
// not result in blacklisting the module if the first frame also was missing
// unwind info. This ensures we don't blacklist an innocent module because the
// first frame was bad but we didn't know it at the time.
TEST_F(Win32StackFrameUnwinderTest, ModuleFromQuestionableFrameNotBlacklisted) {
  const DWORD64 image_base_for_questionable_module = 2048;
  {
    // First stack, with both the first and second frames missing unwind info.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    unwind_functions_.SetNoUnwindInfoForNextFrame();
    EXPECT_TRUE(unwinder->TryUnwind(&context));

    unwind_functions_.SetNoUnwindInfoForNextFrame();
    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_questionable_module);
    EXPECT_FALSE(unwinder->TryUnwind(&context));
  }

  {
    // Second stack; check that the questionable module was not blacklisted.
    scoped_ptr<Win32StackFrameUnwinder> unwinder = CreateUnwinder();
    CONTEXT context = {0};
    unwind_functions_.SetNoUnwindInfoForNextFrame();
    unwind_functions_.SetImageBaseForNextFrame(
        image_base_for_questionable_module);
    EXPECT_TRUE(unwinder->TryUnwind(&context));
  }
}

}  // namespace base
