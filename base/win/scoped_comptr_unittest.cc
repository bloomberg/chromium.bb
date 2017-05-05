// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/scoped_comptr.h"

#include <objbase.h>
#include <shlobj.h>

#include <memory>

#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

struct Dummy {
  Dummy() : adds(0), releases(0) { }
  unsigned long AddRef() { return ++adds; }
  unsigned long Release() { return ++releases; }

  int adds;
  int releases;
};

extern const IID dummy_iid;
const IID dummy_iid = {0x12345678u,
                       0x1234u,
                       0x5678u,
                       {01, 23, 45, 67, 89, 01, 23, 45}};

}  // namespace

TEST(ScopedComPtrTest, ScopedComPtr) {
  base::win::ScopedCOMInitializer com_initializer;
  EXPECT_TRUE(com_initializer.succeeded());

  ScopedComPtr<IUnknown> unk;
  EXPECT_TRUE(SUCCEEDED(unk.CreateInstance(CLSID_ShellLink)));
  ScopedComPtr<IUnknown> unk2;
  unk2.Attach(unk.Detach());
  EXPECT_TRUE(unk.Get() == NULL);
  EXPECT_TRUE(unk2.Get() != NULL);

  ScopedComPtr<IMalloc> mem_alloc;
  EXPECT_TRUE(SUCCEEDED(CoGetMalloc(1, mem_alloc.Receive())));

  ScopedComPtr<IUnknown> qi_test;
  EXPECT_HRESULT_SUCCEEDED(mem_alloc.CopyTo(IID_PPV_ARGS(&qi_test)));
  EXPECT_TRUE(qi_test.Get() != NULL);
  qi_test.Reset();

  // test ScopedComPtr& constructor
  ScopedComPtr<IMalloc> copy1(mem_alloc);
  EXPECT_TRUE(copy1.IsSameObject(mem_alloc.Get()));
  EXPECT_FALSE(copy1.IsSameObject(unk2.Get()));  // unk2 is valid but different
  EXPECT_FALSE(copy1.IsSameObject(unk.Get()));  // unk is NULL

  IMalloc* naked_copy = copy1.Detach();
  copy1 = naked_copy;  // Test the =(T*) operator.
  naked_copy->Release();

  copy1.Reset();
  EXPECT_FALSE(copy1.IsSameObject(unk2.Get()));  // unk2 is valid, copy1 is not

  // test Interface* constructor
  ScopedComPtr<IMalloc> copy2(static_cast<IMalloc*>(mem_alloc.Get()));
  EXPECT_TRUE(copy2.IsSameObject(mem_alloc.Get()));

  EXPECT_TRUE(SUCCEEDED(unk.QueryFrom(mem_alloc.Get())));
  EXPECT_TRUE(unk.Get() != NULL);
  unk.Reset();
  EXPECT_TRUE(unk.Get() == NULL);
  EXPECT_TRUE(unk.IsSameObject(copy1.Get()));  // both are NULL
}

TEST(ScopedComPtrTest, ScopedComPtrVector) {
  // Verify we don't get error C2558.
  typedef ScopedComPtr<Dummy, &dummy_iid> Ptr;
  std::vector<Ptr> bleh;

  std::unique_ptr<Dummy> p(new Dummy);
  {
    Ptr p2(p.get());
    EXPECT_EQ(p->adds, 1);
    EXPECT_EQ(p->releases, 0);
    Ptr p3 = p2;
    EXPECT_EQ(p->adds, 2);
    EXPECT_EQ(p->releases, 0);
    p3 = p2;
    EXPECT_EQ(p->adds, 3);
    EXPECT_EQ(p->releases, 1);
    // To avoid hitting a reallocation.
    bleh.reserve(1);
    bleh.push_back(p2);
    EXPECT_EQ(p->adds, 4);
    EXPECT_EQ(p->releases, 1);
    EXPECT_EQ(bleh[0].Get(), p.get());
    bleh.pop_back();
    EXPECT_EQ(p->adds, 4);
    EXPECT_EQ(p->releases, 2);
  }
  EXPECT_EQ(p->adds, 4);
  EXPECT_EQ(p->releases, 4);
}

}  // namespace win
}  // namespace base
