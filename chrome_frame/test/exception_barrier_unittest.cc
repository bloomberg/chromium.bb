// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gtest/gtest.h"
#include "chrome_frame/exception_barrier.h"

namespace {

// retrieves the top SEH registration record
__declspec(naked) EXCEPTION_REGISTRATION* GetTopRegistration() {
  __asm {
    mov eax, FS:0
    ret
  }
}

// This function walks the SEH chain and attempts to ascertain whether it's
// sane, or rather tests it for any obvious signs of insanity.
// The signs it's capable of looking for are:
//   # Is each exception registration in bounds of our stack
//   # Is the registration DWORD aligned
//   # Does each exception handler point to a module, as opposed to
//      e.g. into the stack or never-never land.
//   # Do successive entries in the exception chain increase
//      monotonically in address
void TestSEHChainSane() {
  // get the skinny on our stack segment
  MEMORY_BASIC_INFORMATION info = { 0 };
  // Note that we pass the address of the info struct just as a handy
  // moniker to anything at all inside our stack allocation
  ASSERT_NE(0u, ::VirtualQuery(&info, &info, sizeof(info)));

  // The lower bound of our stack.
  // We use the address of info as a lower bound, this assumes that if this
  // function has an SEH handler, it'll be higher up in our invocation
  // record.
  EXCEPTION_REGISTRATION* limit =
          reinterpret_cast<EXCEPTION_REGISTRATION*>(&info);
  // the very top of our stack segment
  EXCEPTION_REGISTRATION* top =
          reinterpret_cast<EXCEPTION_REGISTRATION*>(
              reinterpret_cast<char*>(info.BaseAddress) + info.RegionSize);

  EXCEPTION_REGISTRATION* curr = GetTopRegistration();
  // there MUST be at least one registration
  ASSERT_TRUE(NULL != curr);

  EXCEPTION_REGISTRATION* prev = NULL;
  const EXCEPTION_REGISTRATION* kSentinel =
          reinterpret_cast<EXCEPTION_REGISTRATION*>(0xFFFFFFFF);
  for (; kSentinel != curr; prev = curr, curr = curr->prev) {
    // registrations must increase monotonically
    ASSERT_TRUE(curr > prev);
    // Check it's in bounds
    ASSERT_GE(top, curr);
    ASSERT_LT(limit, curr);

    // check for DWORD alignment
    ASSERT_EQ(0, (reinterpret_cast<UINT_PTR>(prev) & 0x00000003));

    // find the module hosting the handler
    ASSERT_NE(0u, ::VirtualQuery(curr->handler, &info, sizeof(info)));
    wchar_t module_filename[MAX_PATH];
    ASSERT_NE(0u, ::GetModuleFileName(
                    reinterpret_cast<HMODULE>(info.AllocationBase),
                    module_filename, ARRAYSIZE(module_filename)));
  }
}

void AccessViolationCrash() {
  volatile char* null = NULL;
  *null = '\0';
}

// A simple crash over the exception barrier
void CrashOverExceptionBarrier() {
  ExceptionBarrierCustomHandler barrier;

  TestSEHChainSane();

  AccessViolationCrash();

  TestSEHChainSane();
}

#pragma warning(push)
// Inline asm assigning to 'FS:0' : handler not registered as safe handler
// This warning is in error (the compiler can't know that we register the
// handler as a safe SEH handler in an .asm file)
#pragma warning(disable:4733)
// Hand-generate an SEH frame implicating the ExceptionBarrierCallCustomHandler,
// then crash to invoke it.
__declspec(naked) void CrashOnManualSEHBarrierHandler() {
  __asm {
    push  ExceptionBarrierCallCustomHandler
    push  FS:0
    mov   FS:0, esp
    call  AccessViolationCrash
    ret
  }
}
#pragma warning(pop)


class ExceptionBarrierTest: public testing::Test {
 public:
  ExceptionBarrierTest() {
  }

  // Install an exception handler for the ExceptionBarrier, and
  // set the handled flag to false. This allows us to see whether
  // the ExceptionBarrier gets to handle the exception
  virtual void SetUp() {
    ExceptionBarrierConfig::set_enabled(true);
    ExceptionBarrierCustomHandler::set_custom_handler(&ExceptionHandler);
    s_handled_ = false;

    TestSEHChainSane();
  }

  virtual void TearDown() {
    TestSEHChainSane();
    ExceptionBarrierCustomHandler::set_custom_handler(NULL);
    ExceptionBarrierConfig::set_enabled(false);
  }

  // The exception notification callback, sets the handled flag.
  static void CALLBACK ExceptionHandler(EXCEPTION_POINTERS* ptrs) {
    TestSEHChainSane();
    s_handled_ = true;
  }

  // Flag is set by handler
  static bool s_handled_;
};

bool ExceptionBarrierTest::s_handled_ = false;

bool TestExceptionExceptionBarrierHandler() {
  TestSEHChainSane();
  __try {
    CrashOnManualSEHBarrierHandler();
    return false;  // not reached
  } __except(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode() ?
                      EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    TestSEHChainSane();
    return true;
  }

  return false;  // not reached
}

typedef EXCEPTION_DISPOSITION
(__cdecl* ExceptionBarrierHandlerFunc)(
    struct _EXCEPTION_RECORD* exception_record,
    void* establisher_frame,
    struct _CONTEXT* context,
    void* reserved);

TEST_F(ExceptionBarrierTest, RegisterUnregister) {
  // Assert that registration modifies the chain
  // and the registered record as expected
  EXCEPTION_REGISTRATION* top = GetTopRegistration();
  ExceptionBarrierHandlerFunc handler = top->handler;
  EXCEPTION_REGISTRATION* prev = top->prev;

  EXCEPTION_REGISTRATION registration;
  ::RegisterExceptionRecord(&registration, ExceptionBarrierHandler);
  EXPECT_EQ(GetTopRegistration(), &registration);
  EXPECT_EQ(&ExceptionBarrierHandler, registration.handler);
  EXPECT_EQ(top, registration.prev);

  // test the whole chain for good measure
  TestSEHChainSane();

  // Assert that unregistration restores
  // everything as expected
  ::UnregisterExceptionRecord(&registration);
  EXPECT_EQ(top, GetTopRegistration());
  EXPECT_EQ(handler, top->handler);
  EXPECT_EQ(prev, top->prev);

  // and again test the whole chain for good measure
  TestSEHChainSane();
}


TEST_F(ExceptionBarrierTest, ExceptionBarrierHandler) {
  EXPECT_TRUE(TestExceptionExceptionBarrierHandler());
  EXPECT_TRUE(s_handled_);
}

bool TestExceptionBarrier() {
  __try {
    CrashOverExceptionBarrier();
  } __except(EXCEPTION_ACCESS_VIOLATION == GetExceptionCode() ?
                      EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
    TestSEHChainSane();
    return true;
  }

  return false;
}

TEST_F(ExceptionBarrierTest, HandlesAccessViolationException) {
  TestExceptionBarrier();
  EXPECT_TRUE(s_handled_);
}

void RecurseAndCrash(int depth) {
  __try {
    __try {
      if (0 == depth)
        AccessViolationCrash();
      else
        RecurseAndCrash(depth - 1);

      TestSEHChainSane();
    } __except(EXCEPTION_CONTINUE_SEARCH) {
      TestSEHChainSane();
    }
  } __finally {
    TestSEHChainSane();
  }
}

// This test exists only for comparison with TestExceptionBarrierChaining, and
// to "document" how the SEH chain is manipulated under our compiler.
// The two tests are expected to both fail if the particulars of the compiler's
// SEH implementation happens to change.
bool TestRegularChaining(EXCEPTION_REGISTRATION* top) {
  // This test relies on compiler-dependent details, notably we rely on the
  // compiler to generate a single SEH frame for the entire function, as
  // opposed to e.g. generating a separate SEH frame for each __try __except
  // statement.
  EXCEPTION_REGISTRATION* my_top = GetTopRegistration();
  if (my_top == top)
    return false;

  __try {
    // we should have the new entry in effect here still
    if (GetTopRegistration() != my_top)
      return false;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }

  __try {
    AccessViolationCrash();
    return false;  // not reached
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // and here
    if (GetTopRegistration() != my_top)
      return false;
  }

  __try {
    RecurseAndCrash(10);
    return false;  // not reached
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // we should have unrolled to our frame by now
    if (GetTopRegistration() != my_top)
      return false;
  }

  return true;
}

void RecurseAndCrashOverBarrier(int depth, bool crash) {
  ExceptionBarrierCustomHandler barrier;

  if (0 == depth) {
    if (crash)
      AccessViolationCrash();
  } else {
    RecurseAndCrashOverBarrier(depth - 1, crash);
  }
}

// Test that ExceptionBarrier doesn't molest the SEH chain, neither
// for regular unwinding, nor on exception unwinding cases.
//
// Note that while this test shows the ExceptionBarrier leaves the chain
// sane on both those cases, it's not clear that it does the right thing
// during first-chance exception handling. I can't think of a way to test
// that though, because first-chance exception handling is very difficult
// to hook into and to observe.
static bool TestExceptionBarrierChaining(EXCEPTION_REGISTRATION* top) {
  TestSEHChainSane();

  // This test relies on compiler-dependent details, notably we rely on the
  // compiler to generate a single SEH frame for the entire function, as
  // opposed to e.g. generating a separate SEH frame for each __try __except
  // statement.
  // Unfortunately we can't use ASSERT macros here, because they create
  // temporary objects and the compiler doesn't grok non POD objects
  // intermingled with __try and other SEH constructs.
  EXCEPTION_REGISTRATION* my_top = GetTopRegistration();
  if (my_top == top)
    return false;

  __try {
    // we should have the new entry in effect here still
    if (GetTopRegistration() != my_top)
      return false;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    return false;  // Not reached
  }

  __try {
    CrashOverExceptionBarrier();
    return false;  // Not reached
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // and here
    if (GetTopRegistration() != my_top)
      return false;
  }
  TestSEHChainSane();

  __try {
    RecurseAndCrashOverBarrier(10, true);
    return false;  // not reached
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    // we should have unrolled to our frame by now
    if (GetTopRegistration() != my_top)
      return false;
  }
  TestSEHChainSane();

  __try {
    RecurseAndCrashOverBarrier(10, false);

    // we should have unrolled to our frame by now
    if (GetTopRegistration() != my_top)
      return false;
  } __except(EXCEPTION_EXECUTE_HANDLER) {
    return false;  // not reached
  }
  TestSEHChainSane();

  // success.
  return true;
}

static bool TestChaining() {
  EXCEPTION_REGISTRATION* top = GetTopRegistration();

  return TestRegularChaining(top) && TestExceptionBarrierChaining(top);
}

// Test that the SEH chain is unmolested by exception barrier, both under
// regular unroll, and under exception unroll.
TEST_F(ExceptionBarrierTest, SEHChainIsSaneAfterException) {
  EXPECT_TRUE(TestChaining());
}

}  // namespace
