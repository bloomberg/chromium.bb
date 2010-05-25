// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "chrome_frame/function_stub.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

// Test subclass to expose extra stuff.
class TestFunctionStub: public FunctionStub {
 public:
  static void Init(TestFunctionStub* stub) {
    stub->FunctionStub::Init(&stub->stub_);
  }

  // Expose the offset to our signature_ field.
  static const size_t kSignatureOffset;

  void set_signature(HMODULE signature) { signature_ = signature; }
};

const size_t TestFunctionStub::kSignatureOffset =
    FIELD_OFFSET(TestFunctionStub, signature_);

class FunctionStubTest: public testing::Test {
 public:
  FunctionStubTest() : stub_(NULL) {
  }

  virtual void SetUp() {
    SYSTEM_INFO sys_info;
    ::GetSystemInfo(&sys_info);

    // Playpen size is a system page.
    playpen_size_ = sys_info.dwPageSize;

    // Reserve two pages.
    playpen_ = reinterpret_cast<uint8*>(
        ::VirtualAlloc(NULL,
                       2 * playpen_size_,
                       MEM_RESERVE,
                       PAGE_EXECUTE_READWRITE));
    ASSERT_TRUE(playpen_ != NULL);

    // And commit the first one.
    ASSERT_TRUE(::VirtualAlloc(playpen_,
                               playpen_size_,
                               MEM_COMMIT,
                               PAGE_EXECUTE_READWRITE));
  }

  virtual void TearDown() {
    if (stub_ != NULL) {
      EXPECT_TRUE(FunctionStub::Destroy(stub_));
    }

    if (playpen_ != NULL) {
      EXPECT_TRUE(::VirtualFree(playpen_, 0, MEM_RELEASE));
    }
  }

 protected:
  typedef uintptr_t (CALLBACK *FuncPtr0)();
  typedef uintptr_t (CALLBACK *FuncPtr1)(uintptr_t arg);

  MOCK_METHOD0(Foo0, uintptr_t());
  MOCK_METHOD1(Foo1, uintptr_t(uintptr_t));
  MOCK_METHOD0(Bar0, uintptr_t());
  MOCK_METHOD1(Bar1, uintptr_t(uintptr_t));

  static uintptr_t CALLBACK FooCallback0(FunctionStubTest* test) {
    return test->Foo0();
  }
  static uintptr_t CALLBACK FooCallback1(FunctionStubTest* test,
                                         uintptr_t arg) {
    return test->Foo1(arg);
  }
  static uintptr_t CALLBACK BarCallback0(FunctionStubTest* test) {
    return test->Foo0();
  }
  static uintptr_t CALLBACK BarCallback1(FunctionStubTest* test,
                                         uintptr_t arg) {
    return test->Foo1(arg);
  }

  // If a stub is allocated during testing, assigning it here
  // will deallocate it at the end of test.
  FunctionStub* stub_;

  // playpen_[0 .. playpen_size_ - 1] is committed, writable memory.
  // playpen_[playpen_size_] is uncommitted, defined memory.
  uint8* playpen_;
  size_t playpen_size_;
};

const uintptr_t kDivertedRetVal = 0x42;
const uintptr_t kFooRetVal = 0xCAFEBABE;
const uintptr_t kFooArg = 0xF0F0F0F0;

uintptr_t CALLBACK Foo() {
  return kFooRetVal;
}

uintptr_t CALLBACK FooDivert(uintptr_t arg) {
  return kFooRetVal;
}

}  // namespace

TEST_F(FunctionStubTest, Accessors) {
  uintptr_t argument = reinterpret_cast<uintptr_t>(this);
  uintptr_t dest_fn = reinterpret_cast<uintptr_t>(FooDivert);
  stub_ = FunctionStub::Create(argument, FooDivert);

  EXPECT_FALSE(stub_->is_bypassed());
  EXPECT_TRUE(stub_->is_valid());
  EXPECT_TRUE(stub_->code() != NULL);

  // Check that the stub code is executable.
  MEMORY_BASIC_INFORMATION info = {};
  EXPECT_NE(0u, ::VirtualQuery(stub_->code(), &info, sizeof(info)));
  const DWORD kExecutableMask = PAGE_EXECUTE | PAGE_EXECUTE_READ |
      PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  EXPECT_NE(0u, info.Protect & kExecutableMask);

  EXPECT_EQ(argument, stub_->argument());
  EXPECT_TRUE(stub_->bypass_address() != NULL);
  EXPECT_EQ(dest_fn, stub_->destination_function());
}

TEST_F(FunctionStubTest, ZeroArgumentStub) {
  stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                               &FunctionStubTest::FooCallback0);

  FuncPtr0 func = reinterpret_cast<FuncPtr0>(stub_->code());
  EXPECT_CALL(*this, Foo0())
      .WillOnce(testing::Return(kDivertedRetVal));

  EXPECT_EQ(kDivertedRetVal, func());
}

TEST_F(FunctionStubTest, OneArgumentStub) {
  stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                               &FunctionStubTest::FooCallback1);

  FuncPtr1 func = reinterpret_cast<FuncPtr1>(stub_->code());
  EXPECT_CALL(*this, Foo1(kFooArg))
      .WillOnce(testing::Return(kDivertedRetVal));

  EXPECT_EQ(kDivertedRetVal, func(kFooArg));
}

TEST_F(FunctionStubTest, Bypass) {
  stub_ = FunctionStub::Create(reinterpret_cast<uintptr_t>(this),
                               &FunctionStubTest::FooCallback0);

  FuncPtr0 func = reinterpret_cast<FuncPtr0>(stub_->code());
  EXPECT_CALL(*this, Foo0())
      .WillOnce(testing::Return(kDivertedRetVal));

  // This will call through to foo.
  EXPECT_EQ(kDivertedRetVal, func());

  // Now bypass to Foo().
  stub_->BypassStub(Foo);
  EXPECT_TRUE(stub_->is_bypassed());
  EXPECT_FALSE(stub_->is_valid());

  // We should not call through anymore.
  EXPECT_CALL(*this, Foo0())
      .Times(0);

  EXPECT_EQ(kFooRetVal, func());
}

TEST_F(FunctionStubTest, FromCode) {
  // We should get NULL and no crash from reserved memory.
  EXPECT_EQ(NULL, FunctionStub::FromCode(playpen_ + playpen_size_));

  // Create a FunctionStub pointer whose signature_
  // field hangs just off the playpen.
  TestFunctionStub* stub =
      reinterpret_cast<TestFunctionStub*>(playpen_ + playpen_size_ -
                                          TestFunctionStub::kSignatureOffset);
  TestFunctionStub::Init(stub);
  EXPECT_EQ(NULL, FunctionStub::FromCode(stub));

  // Create a stub in committed memory.
  stub = reinterpret_cast<TestFunctionStub*>(playpen_);
  TestFunctionStub::Init(stub);
  // Signature is NULL, which won't do.
  EXPECT_EQ(NULL, FunctionStub::FromCode(stub));

  const DWORD kFlags = GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
      GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT;

  HMODULE my_module = NULL;
  EXPECT_TRUE(::GetModuleHandleEx(kFlags,
      reinterpret_cast<const wchar_t*>(&kDivertedRetVal),
      &my_module));

  // Set our module as signature.
  stub->set_signature(my_module);
  EXPECT_EQ(stub, FunctionStub::FromCode(stub));
}

