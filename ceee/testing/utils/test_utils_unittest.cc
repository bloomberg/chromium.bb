// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for test utils.

#include "ceee/testing/utils/test_utils.h"

#include "base/scoped_vector.h"
#include "ceee/common/initializing_coclass.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(LogDisabler, ItDisables) {
  testing::LogDisabler disabler;
  DCHECK(false);
}

}  // namespace

namespace {

using testing::_;
using testing::CopyBSTRToArgument;
using testing::CopyInterfaceToArgument;
using testing::CopyStringToArgument;
using testing::CopyVariantToArgument;
using testing::DoAll;
using testing::Return;

class IInt : public IUnknown {
 public:
  virtual int get() const = 0;
  virtual void set(int i) = 0;
};

// {FA26DE41-C212-4e11-A23F-C7DC405180A2}
const GUID IID_IInt =
  { 0xfa26de41, 0xc212, 0x4e11,
    { 0xa2, 0x3f, 0xc7, 0xdc, 0x40, 0x51, 0x80, 0xa2 } };

class MockInt
    : public CComObjectRootEx<CComSingleThreadModel>,
      public IInt {
 public:
  BEGIN_COM_MAP(MockInt)
    COM_INTERFACE_ENTRY_IID(IID_IInt, IInt)
  END_COM_MAP()

  HRESULT Initialize(MockInt** i) {
    *i = this;
    return S_OK;
  }

  int get() const { return i_; }
  void set(int i) { i_ = i; }

 private:
  int i_;
};

class MockGetter {
 public:
  MOCK_METHOD1(GetString, void(wchar_t* str));
  MOCK_METHOD1(GetBSTR, void(BSTR* str));
  MOCK_METHOD1(GetIInt, void(IInt** i));
  MOCK_METHOD1(GetVariant, void(VARIANT* var));
};

const wchar_t* kStr = L"hello";
const int kInt = 5;

TEST(CopyStringToArgument, ItCopies) {
  MockGetter getter;

  EXPECT_CALL(getter, GetString(_)).WillOnce(CopyStringToArgument<0>(kStr));

  wchar_t* ret = new wchar_t[lstrlenW(kStr) + 1];
  getter.GetString(ret);
  ASSERT_EQ(wcscmp(ret, kStr), 0);
  delete[] ret;
}

TEST(CopyBSTRToArgument, ItCopies) {
  MockGetter getter;

  EXPECT_CALL(getter, GetBSTR(_)).WillOnce(CopyBSTRToArgument<0>(kStr));

  CComBSTR expected = kStr;
  CComBSTR ret;
  getter.GetBSTR(&ret);
  ASSERT_EQ(ret, expected);
}

TEST(CopyInterfaceToArgument, ItCopies) {
  MockGetter getter;

  {
    // Cause ptr to go out of scope.
    MockInt* mock_int;
    CComPtr<IInt> int_ptr;
    ASSERT_HRESULT_SUCCEEDED(
          InitializingCoClass<MockInt>::CreateInitializedIID(
              &mock_int, IID_IInt, &int_ptr));
    int_ptr->set(kInt);

    EXPECT_CALL(getter, GetIInt(_))
        .WillOnce(CopyInterfaceToArgument<0>(int_ptr));
  }

  CComPtr<IInt> int_ptr;
  getter.GetIInt(&int_ptr);
  ASSERT_EQ(int_ptr->get(), kInt);
}

TEST(CopyVariantToArgument, ItCopies) {
  MockGetter getter;

  {
    // Cause the variant to go out of scope.
    MockInt* mock_int;
    CComPtr<IInt> int_ptr;
    ASSERT_HRESULT_SUCCEEDED(
          InitializingCoClass<MockInt>::CreateInitializedIID(
              &mock_int, IID_IInt, &int_ptr));
    int_ptr->set(kInt);

    CComVariant var(int_ptr);
    EXPECT_CALL(getter, GetVariant(_)).WillOnce(CopyVariantToArgument<0>(var));
  }

  CComVariant ret;
  getter.GetVariant(&ret);
  ASSERT_EQ(V_VT(&ret), VT_UNKNOWN);

  CComPtr<IInt> int_ptr;
  ASSERT_HRESULT_SUCCEEDED(V_UNKNOWN(&ret)->QueryInterface(
        IID_IInt, reinterpret_cast<void**>(&int_ptr)));
  ASSERT_EQ(int_ptr->get(), kInt);
}

}  // namespace
