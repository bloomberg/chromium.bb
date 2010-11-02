// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for PreamblePatcher

#include "ceee/testing/sidestep/integration.h"
#include "ceee/testing/sidestep/preamble_patcher.h"
#include "ceee/testing/sidestep/mini_disassembler.h"
#pragma warning(push)
#pragma warning(disable:4553)
#include "ceee/testing/sidestep/auto_testing_hook.h"
#pragma warning(pop)

// Turning off all optimizations for this file, since the official build's
// "Whole program optimization" seems to cause the TestPatchUsingDynamicStub
// test to crash with an access violation.  We debugged this and found
// that the optimized access a register that is changed by a call to the hook
// function.
#pragma optimize("", off)

namespace {

// Function for testing - this is what we patch
//
// NOTE:  Because of the way the compiler optimizes this function in
// release builds, we need to use a different input value every time we
// call it within a function, otherwise the compiler will just reuse the
// last calculated incremented value.
int __declspec(noinline) IncrementNumber(int i) {
  return i + 1;
}

// A function that is too short to patch
int __declspec(naked) TooShortFunction(int) {
  __asm {
    ret
        }
}

typedef int (*IncrementingFunc)(int);
IncrementingFunc original_function = NULL;

int HookIncrementNumber(int i) {
  SIDESTEP_ASSERT(original_function != NULL);
  int incremented_once = original_function(i);
  return incremented_once + 1;
}

// For the AutoTestingHook test, we can't use original_function, because
// all that is encapsulated.
// This function "increments" by 10, just to set it apart from the other
// functions.
int __declspec(noinline) AutoHookIncrementNumber(int i) {
  return i + 10;
}

};  // namespace

namespace sidestep {

bool TestPatchUsingDynamicStub() {
  original_function = NULL;
  SIDESTEP_EXPECT_TRUE(IncrementNumber(1) == 2);
  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_SUCCESS ==
                       sidestep::PreamblePatcher::Patch(IncrementNumber,
                                                        HookIncrementNumber,
                                                        &original_function));
  SIDESTEP_EXPECT_TRUE(original_function);
  SIDESTEP_EXPECT_TRUE(IncrementNumber(2) == 4);
  SIDESTEP_EXPECT_TRUE(original_function(3) == 4);

  // Clearbox test to see that the function has been patched.
  sidestep::MiniDisassembler disassembler;
  unsigned int instruction_size = 0;
  SIDESTEP_EXPECT_TRUE(sidestep::IT_JUMP == disassembler.Disassemble(
                           reinterpret_cast<unsigned char*>(IncrementNumber),
                           instruction_size));

  // Since we patched IncrementNumber, its first statement is a
  // jmp to the hook function.  So verify that we now can not patch
  // IncrementNumber because it starts with a jump.
#if 0
  IncrementingFunc dummy = NULL;
  // TODO(joi@chromium.org): restore this test once flag is added to
  // disable JMP following
  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_JUMP_INSTRUCTION ==
                       sidestep::PreamblePatcher::Patch(IncrementNumber,
                                                        HookIncrementNumber,
                                                        &dummy));

  // This test disabled because code in preamble_patcher_with_stub.cc
  // asserts before returning the error code -- so there is no way
  // to get an error code here, in debug build.
  dummy = NULL;
  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_FUNCTION_TOO_SMALL ==
                       sidestep::PreamblePatcher::Patch(TooShortFunction,
                                                        HookIncrementNumber,
                                                        &dummy));
#endif

  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_SUCCESS ==
                       sidestep::PreamblePatcher::Unpatch(IncrementNumber,
                                                          HookIncrementNumber,
                                                          original_function));
  return true;
}

bool PatchThenUnpatch() {
  original_function = NULL;
  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_SUCCESS ==
                       sidestep::PreamblePatcher::Patch(IncrementNumber,
                                                        HookIncrementNumber,
                                                        &original_function));
  SIDESTEP_EXPECT_TRUE(original_function);
  SIDESTEP_EXPECT_TRUE(IncrementNumber(1) == 3);
  SIDESTEP_EXPECT_TRUE(original_function(2) == 3);

  SIDESTEP_EXPECT_TRUE(sidestep::SIDESTEP_SUCCESS ==
                       sidestep::PreamblePatcher::Unpatch(IncrementNumber,
                                                          HookIncrementNumber,
                                                          original_function));
  original_function = NULL;
  SIDESTEP_EXPECT_TRUE(IncrementNumber(3) == 4);

  return true;
}

bool AutoTestingHookTest() {
  SIDESTEP_EXPECT_TRUE(IncrementNumber(1) == 2);

  // Inner scope, so we can test what happens when the AutoTestingHook
  // goes out of scope
  {
    AutoTestingHook hook = MakeTestingHook(IncrementNumber,
                                           AutoHookIncrementNumber);
    (void) hook;
    SIDESTEP_EXPECT_TRUE(IncrementNumber(2) == 12);
  }
  SIDESTEP_EXPECT_TRUE(IncrementNumber(3) == 4);

  return true;
}

bool AutoTestingHookInContainerTest() {
  SIDESTEP_EXPECT_TRUE(IncrementNumber(1) == 2);

  // Inner scope, so we can test what happens when the AutoTestingHook
  // goes out of scope
  {
    AutoTestingHookHolder hook(MakeTestingHookHolder(IncrementNumber,
                                                     AutoHookIncrementNumber));
    (void) hook;
    SIDESTEP_EXPECT_TRUE(IncrementNumber(2) == 12);
  }
  SIDESTEP_EXPECT_TRUE(IncrementNumber(3) == 4);

  return true;
}

bool UnitTests() {
  return TestPatchUsingDynamicStub() && PatchThenUnpatch() &&
      AutoTestingHookTest() && AutoTestingHookInContainerTest();
}

};  // namespace sidestep

#pragma optimize("", on)
