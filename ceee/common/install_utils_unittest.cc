// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for utilities related to installation of the CEEE.

#include "ceee/common/install_utils.h"

#include "base/base_paths.h"
#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::CopyStringToArgument;
using testing::Return;

TEST(ShouldRegisterCeee, FalseUnlessRegsvr32OrCommandLine) {
  // We can safely assume that the real command line and
  // process name for the unit test process does not satisfy
  // either condition.
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
}

TEST(ShouldRegisterCeee, TrueIfRegsvr32) {
  testing::PathServiceOverrider override(base::FILE_EXE,
      FilePath(L"c:\\windows\\system32\\regsvr32.exe"));
  ASSERT_TRUE(ceee_install_utils::ShouldRegisterCeee());
  ASSERT_TRUE(ceee_install_utils::ShouldRegisterFfCeee());
}

TEST(ShouldRegisterCeee, FalseIfRegsvr32NotLastPathComponent) {
  testing::PathServiceOverrider override(base::FILE_EXE,
    FilePath(L"c:\\windows\\regsvr32.exe\\foobar.exe"));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
}

TEST(ShouldRegisterCeee, TrueIfBothFlagsPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).WillOnce(
      Return(const_cast<wchar_t*>(
          L"mini_installer.exe --enable-ceee --chrome-frame")));
  ASSERT_TRUE(ceee_install_utils::ShouldRegisterCeee());
}

TEST(ShouldRegisterFfCeee, TrueIfAllFlagsPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).Times(2).WillRepeatedly(
      Return(const_cast<wchar_t*>(
          L"mini_installer.exe --enable-ceee --chrome-frame "
          L"--enable-ff-ceee")));
  EXPECT_TRUE(ceee_install_utils::ShouldRegisterFfCeee());
  // Check this as well just in case.
  ASSERT_TRUE(ceee_install_utils::ShouldRegisterCeee());
}

TEST(ShouldRegisterCeee, FalseIfOneFlagPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine())
    .WillOnce(Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceee")))
    .WillOnce(Return(const_cast<wchar_t*>(
        L"mini_installer.exe --chrome-frame")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
}

TEST(ShouldRegisterFfCeee, FalseIfOnlyTwoFlagsPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine())
    .WillOnce(Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceee --chrome-frame")))
    .WillOnce(Return(const_cast<wchar_t*>(
        L"mini_installer.exe --chrome-frame --enable-ff-ceee")))
    .WillOnce(Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceee --enable-ff-ceee")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
}

TEST(ShouldRegisterCeee, FalseIfInvertedFlagPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).WillOnce(
    Return(const_cast<wchar_t*>(
        L"mini_installer.exe --noenable-ceee --chrome-frame")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
}

TEST(ShouldRegisterFfCeee, FalseIfInvertedFlagPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).WillOnce(
    Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceee --noenable-ff-ceee "
        L"--chrome-frame")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
}

TEST(ShouldRegisterCeee, FalseIfBogusFlagPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).WillOnce(
    Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceeez --chrome-frame")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterCeee());
}

TEST(ShouldRegisterFfCeee, FalseIfBogusFlagPresent) {
  testing::MockKernel32 kernel32;
  EXPECT_CALL(kernel32, GetCommandLine()).WillOnce(
    Return(const_cast<wchar_t*>(
        L"mini_installer.exe --enable-ceee --enable-ff-ceeez "
        L"--chrome-frame")));
  ASSERT_FALSE(ceee_install_utils::ShouldRegisterFfCeee());
}

namespace dll_register_server_test {

// Gets set to true by DllRegisterServer.
bool registration_was_done = false;

// This is for unit testing only. Because we're in an anonymous namespace
// it's separate from everything else.
STDAPI DllRegisterServerImpl() {
  registration_was_done = true;
  return S_OK;
}

CEEE_DEFINE_DLL_REGISTER_SERVER()

TEST(ConditionalDllRegisterServer, ShouldNotInstall) {
  // We can safely assume that the real command line and
  // process name for the unit test process does not satisfy
  // either condition.
  registration_was_done = false;
  DllRegisterServer();
  ASSERT_FALSE(registration_was_done);
}

TEST(ConditionalDllRegisterServer, ShouldInstall) {
  // This is just one way to make ShouldRegisterCeee()
  // return true; we've unit tested elsewhere that the other cases
  // are handled correctly.
  testing::PathServiceOverrider override(base::FILE_EXE,
      FilePath(L"c:\\windows\\system32\\regsvr32.exe"));

  registration_was_done = false;
  DllRegisterServer();
  ASSERT_TRUE(registration_was_done);
}

}  // namespace dll_register_server_test


}  // namespace
