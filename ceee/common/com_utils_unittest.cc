// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for COM utils.

#include "ceee/common/com_utils.h"

#include <atlcomcli.h>

#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/mock_static.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace com {

HRESULT UpdateRegistryFromResourceImpl(int reg_id, BOOL should_register,
                                       _ATL_REGMAP_ENTRY* entries);

}  // namespace com;

namespace {

using testing::_;
using testing::DoAll;
using testing::CopyStringToArgument;
using testing::Field;
using testing::Pointee;
using testing::Return;
using testing::StrEq;

TEST(ComUtils, AlwaysError) {
  EXPECT_EQ(com::AlwaysError(S_OK, E_INVALIDARG), E_INVALIDARG);
  EXPECT_EQ(com::AlwaysError(E_FAIL, E_INVALIDARG), E_FAIL);

  EXPECT_EQ(com::AlwaysError(S_OK), E_FAIL);
  EXPECT_EQ(com::AlwaysError(E_FAIL), E_FAIL);

  EXPECT_EQ(com::AlwaysErrorFromWin32(NO_ERROR, E_INVALIDARG), E_INVALIDARG);
  EXPECT_EQ(com::AlwaysErrorFromWin32(NO_ERROR), E_FAIL);

  EXPECT_EQ(com::AlwaysErrorFromWin32(ERROR_ACCESS_DENIED, E_INVALIDARG),
            HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
  EXPECT_EQ(com::AlwaysErrorFromWin32(ERROR_ACCESS_DENIED),
            HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));

  DWORD last_error = ::GetLastError();

  ::SetLastError(NO_ERROR);
  EXPECT_EQ(com::AlwaysErrorFromLastError(E_INVALIDARG), E_INVALIDARG);
  EXPECT_EQ(com::AlwaysErrorFromLastError(), E_FAIL);

  ::SetLastError(ERROR_ACCESS_DENIED);
  EXPECT_EQ(com::AlwaysErrorFromLastError(E_INVALIDARG),
            HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));
  EXPECT_EQ(com::AlwaysErrorFromLastError(),
            HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED));

  ::SetLastError(last_error);
}

TEST(ComUtils, ToString) {
  CComBSTR filled(L"hello");
  EXPECT_STREQ(com::ToString(filled), L"hello");

  CComBSTR empty;
  EXPECT_STREQ(com::ToString(empty), L"");
}

TEST(ComUtils, HrLog) {
  {
    std::ostringstream stream;
    stream << com::LogHr(S_OK);
    std::string str = stream.str();
    EXPECT_NE(str.find("0x0,"), std::string::npos);
    EXPECT_NE(str.find("msg="), std::string::npos);
  }

  {
    std::ostringstream stream;
    stream << com::LogHr(E_FAIL);
    std::string str = stream.str();
    EXPECT_NE(str.find("0x80004005,"), std::string::npos);
    EXPECT_NE(str.find("msg=Unspecified error"), std::string::npos);
  }
}

TEST(ComUtils, WeLog) {
  {
    std::ostringstream stream;
    stream << com::LogWe(ERROR_SUCCESS);
    std::string str = stream.str();
    EXPECT_NE(str.find("[we=0,"), std::string::npos);
    EXPECT_NE(str.find("msg=The operation completed successfully"),
              std::string::npos);
  }

  {
    std::ostringstream stream;
    stream << com::LogWe(ERROR_INVALID_FUNCTION);
    std::string str = stream.str();
    EXPECT_NE(str.find("[we=1,"), std::string::npos);
    EXPECT_NE(str.find("msg=Incorrect function"), std::string::npos);
  }
}

HRESULT UpdateRegistryFromResourceImpl(int reg_id, BOOL should_register,
                                       _ATL_REGMAP_ENTRY* entries) {
  return _pAtlModule->UpdateRegistryFromResource(
      reg_id, should_register, entries);
}

// Mock out UpdateRegistryFromResourceImpl.
MOCK_STATIC_CLASS_BEGIN(MockModuleRegistrationWithoutAppid)
  MOCK_STATIC_INIT_BEGIN(MockModuleRegistrationWithoutAppid)
    MOCK_STATIC_INIT2(com::UpdateRegistryFromResourceImpl,
                      UpdateRegistryFromResourceImpl);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC3(HRESULT, , UpdateRegistryFromResourceImpl,
               int, BOOL, _ATL_REGMAP_ENTRY*);
MOCK_STATIC_CLASS_END(MockModuleRegistrationWithoutAppid)

MATCHER(ValidateRegmapComponents, "Checks regmap for correct values") {
  return 0 == lstrcmpW(arg[0].szData, L"c:\\foo\\blat") &&
      0 == lstrcmpW(arg[1].szData, L"bingo.exe");
}

TEST_DEBUG_ONLY(ComUtils, ModuleRegistrationWithoutAppid) {
  MockModuleRegistrationWithoutAppid mock;
  EXPECT_CALL(mock, UpdateRegistryFromResourceImpl(
      42, TRUE, ValidateRegmapComponents())).WillRepeatedly(Return(S_OK));

  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetModuleFileName(_, _, _)).WillRepeatedly(DoAll(
      CopyStringToArgument<1>(L"c:\\foo\\blat\\bingo.exe"), Return(1)));

  EXPECT_EQ(S_OK, com::ModuleRegistrationWithoutAppid(42, TRUE));
}

}  // namespace
