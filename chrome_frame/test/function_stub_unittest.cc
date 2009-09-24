// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_frame/function_stub.h"

#include "chrome_frame/test/chrome_frame_unittests.h"

#define NO_INLINE __declspec(noinline)

namespace {

typedef int (__stdcall* FooPrototype)();

NO_INLINE int __stdcall Foo() {
  return 1;
}

NO_INLINE int __stdcall PatchedFoo(FooPrototype original) {
  return original() + 1;
}

}  // end namespace

TEST(PatchTests, FunctionStub) {
  EXPECT_EQ(Foo(), 1);
  // Create a function stub that calls PatchedFoo and supplies it with
  // a pointer to Foo.
  FunctionStub* stub = FunctionStub::Create(reinterpret_cast<uintptr_t>(&Foo),
                                            &PatchedFoo);
  EXPECT_TRUE(stub != NULL);
  // Call the stub as it were Foo().  The call should get forwarded to Foo().
  FooPrototype patch = reinterpret_cast<FooPrototype>(stub->code());
  EXPECT_EQ(patch(), 2);
  // Now neutralize the stub so that it calls Foo() directly without touching
  // PatchedFoo().
  // stub->BypassStub(&Foo);
  stub->BypassStub(reinterpret_cast<void*>(stub->argument()));
  EXPECT_EQ(patch(), 1);
  // We're done with the stub.
  FunctionStub::Destroy(stub);
}

// Basic tests to check the validity of a stub.
TEST(PatchTests, FunctionStubCompare) {
  EXPECT_EQ(Foo(), 1);

  // Detect the absence of a stub
  FunctionStub* stub = reinterpret_cast<FunctionStub*>(&Foo);
  EXPECT_FALSE(stub->is_valid());

  stub = FunctionStub::Create(reinterpret_cast<uintptr_t>(&Foo), &PatchedFoo);
  EXPECT_TRUE(stub != NULL);
  EXPECT_TRUE(stub->is_valid());

  FooPrototype patch = reinterpret_cast<FooPrototype>(stub->code());
  EXPECT_EQ(patch(), 2);

  // See if we can get the correct absolute pointer to the hook function
  // back from the stub.
  EXPECT_EQ(stub->absolute_target(), reinterpret_cast<uintptr_t>(&PatchedFoo));

  // Also verify that the argument being passed to the hook function is indeed
  // the pointer to the original function (again, absolute not relative).
  EXPECT_EQ(stub->argument(), reinterpret_cast<uintptr_t>(&Foo));
}
