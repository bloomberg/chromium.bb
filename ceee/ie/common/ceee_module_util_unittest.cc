// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the CEEE module-wide utilities.

#include "ceee/ie/common/ceee_module_util.h"

#include <wtypes.h>
#include <string>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/win/registry.h"
#include "base/string_util.h"
#include "ceee/common/process_utils_win.h"
#include "ceee/ie/testing/mock_broker_and_friends.h"
#include "ceee/testing/utils/mock_com.h"
#include "ceee/testing/utils/mock_window_utils.h"
#include "ceee/testing/utils/mock_win32.h"
#include "ceee/testing/utils/test_utils.h"
#include "chrome/installer/util/google_update_constants.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace {

using testing::_;
using testing::DoAll;
using testing::NotNull;
using testing::SetArgumentPointee;
using testing::StrictMock;
using testing::Return;

const wchar_t* kReplacementRoot =
  L"Software\\Google\\InstallUtilUnittest";
const wchar_t* kHKCUReplacement =
  L"Software\\Google\\InstallUtilUnittest\\HKCU";
const wchar_t* kHKLMReplacement =
  L"Software\\Google\\InstallUtilUnittest\\HKLM";

MOCK_STATIC_CLASS_BEGIN(MockProcessWinUtils)
  MOCK_STATIC_INIT_BEGIN(MockProcessWinUtils)
    MOCK_STATIC_INIT2(process_utils_win::IsCurrentProcessUacElevated,
                      IsCurrentProcessUacElevated);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC1(HRESULT, , IsCurrentProcessUacElevated, bool*);
MOCK_STATIC_CLASS_END(MockProcessWinUtils)


class CeeeModuleUtilTest : public testing::Test {
 protected:
  static const DWORD kFalse = 0;
  static const DWORD kTrue = 1;
  static const DWORD kInvalid = 5;

  virtual void SetUp() {
    // Wipe the keys we redirect to.
    // This gives us a stable run, even in the presence of previous
    // crashes or failures.
    LSTATUS err = SHDeleteKey(HKEY_CURRENT_USER, kReplacementRoot);
    EXPECT_TRUE(err == ERROR_SUCCESS || err == ERROR_FILE_NOT_FOUND);

    // Create the keys we're redirecting HKCU and HKLM to.
    ASSERT_TRUE(hkcu_.Create(HKEY_CURRENT_USER, kHKCUReplacement, KEY_READ));
    ASSERT_TRUE(hklm_.Create(HKEY_CURRENT_USER, kHKLMReplacement, KEY_READ));

    // And do the switcharoo.
    ASSERT_EQ(ERROR_SUCCESS,
      ::RegOverridePredefKey(HKEY_CURRENT_USER, hkcu_.Handle()));
    ASSERT_EQ(ERROR_SUCCESS,
      ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, hklm_.Handle()));
  }

  virtual void TearDown() {
    // Undo the redirection.
    EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_CURRENT_USER, NULL));
    EXPECT_EQ(ERROR_SUCCESS, ::RegOverridePredefKey(HKEY_LOCAL_MACHINE, NULL));

    // Close our handles and delete the temp keys we redirected to.
    hkcu_.Close();
    hklm_.Close();
    EXPECT_EQ(ERROR_SUCCESS, SHDeleteKey(HKEY_CURRENT_USER, kReplacementRoot));
  }

  base::win::RegKey hkcu_;
  base::win::RegKey hklm_;
};

// Mock PathService::Get that is used to look up files.
MOCK_STATIC_CLASS_BEGIN(MockPathService)
  MOCK_STATIC_INIT_BEGIN(MockPathService)
    bool (*func_ptr)(int, FilePath*) = PathService::Get;
    MOCK_STATIC_INIT2(func_ptr, Get);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC2(bool, , Get, int, FilePath*);
MOCK_STATIC_CLASS_END(MockPathService);

// Empty registry case (without crx)
TEST_F(CeeeModuleUtilTest, ExtensionPathTestNoRegistry) {
  namespace cmu = ceee_module_util;

  StrictMock<MockPathService> mock_path;
  FilePath temp_path;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(L"CeeeModuleUtilTest",
                                                &temp_path));

  EXPECT_CALL(mock_path, Get(base::DIR_PROGRAM_FILES, NotNull())).
    WillOnce(DoAll(
        SetArgumentPointee<1>(temp_path),
        Return(true)));
  EXPECT_EQ(std::wstring(), cmu::GetExtensionPath());

  FilePath full_path = temp_path.Append(L"Google").
      Append(L"CEEE").Append(L"Extensions");
  FilePath crx_path = full_path.Append(L"testing.crx");
  ASSERT_TRUE(file_util::CreateDirectory(full_path));
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(full_path, &temp_file_path));
  ASSERT_TRUE(file_util::Move(temp_file_path, crx_path));
  EXPECT_CALL(mock_path, Get(base::DIR_PROGRAM_FILES, NotNull())).
      WillOnce(DoAll(SetArgumentPointee<1>(temp_path), Return(true)));
  EXPECT_EQ(crx_path.value(), cmu::GetExtensionPath());

  // Clean up.
  file_util::Delete(temp_path, true);
}

// http://code.google.com/p/chromium/issues/detail?id=62856
TEST_F(CeeeModuleUtilTest, FLAKY_ExtensionPathTest) {
  namespace cmu = ceee_module_util;

  // The FilePath::Get method shouldn't be called if we take the value
  // from the registry.
  StrictMock<MockPathService> mock_path;

  // Creates a registry key
  base::win::RegKey hkcu(HKEY_CURRENT_USER, L"SOFTWARE\\Google\\CEEE",
                         KEY_WRITE);
  base::win::RegKey hklm(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Google\\CEEE",
                         KEY_WRITE);

  // Create a random string so we can compare the result with it.
  // Ideally, this would be a really random string, not some arbitrary constant.
  std::wstring path = L"asdfqwerasdfqwer";
  const wchar_t* kRegistryValueName = L"crx_path";

  hklm.WriteValue(kRegistryValueName, path.c_str());
  hklm.Close();
  EXPECT_EQ(path, cmu::GetExtensionPath());

  // Change the random string and verify we get the new one if we set it
  // to HKCU
  path += L"_HKCU";
  hkcu.WriteValue(kRegistryValueName, path.c_str());
  hkcu.Close();
  EXPECT_EQ(path, cmu::GetExtensionPath());
}

TEST_F(CeeeModuleUtilTest, NeedToInstallExtension) {
  namespace cmu = ceee_module_util;

  // Create our registry key and a temporary file
  base::win::RegKey hkcu(HKEY_CURRENT_USER, L"SOFTWARE\\Google\\CEEE",
                         KEY_WRITE);
  FilePath temp_path;
  ASSERT_TRUE(file_util::CreateNewTempDirectory(L"CeeeModuleUtilTest",
                                                &temp_path));
  FilePath crx_path = temp_path.Append(L"temp.crx");
  FilePath temp_file_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_path, &temp_file_path));
  ASSERT_TRUE(file_util::Move(temp_file_path, crx_path));

  const wchar_t* kRegistryCrxPathValueName = L"crx_path";
  const wchar_t* kRegistryValueCrxInstalledPath = L"crx_installed_path";
  const wchar_t* kRegistryValueCrxInstalledTime = L"crx_installed_time";

  hkcu.WriteValue(kRegistryCrxPathValueName, crx_path.value().c_str());

  // Those should return empty values
  // We use EXPECT_TRUE instead of EXPECT_EQ because there's no operator<<
  // declared for FilePath and base::Time so we cannot output the
  // expected/received values.
  EXPECT_TRUE(FilePath() == cmu::GetInstalledExtensionPath());
  EXPECT_TRUE(base::Time() == cmu::GetInstalledExtensionTime());

  // Now we should need to install, since the keys aren't there yet.
  EXPECT_TRUE(cmu::NeedToInstallExtension());

  // And if we update the install info, we shouldn't need to anymore.
  cmu::SetInstalledExtensionPath(crx_path);
  EXPECT_FALSE(cmu::NeedToInstallExtension());

  // We get the installed path and time and verify them against our values.
  base::PlatformFileInfo crx_info;
  ASSERT_TRUE(file_util::GetFileInfo(crx_path, &crx_info));
  EXPECT_TRUE(crx_path == cmu::GetInstalledExtensionPath());
  EXPECT_TRUE(crx_info.last_modified == cmu::GetInstalledExtensionTime());

  // Finally, if we update the file time, we should be able to install again.
  // But we must make sure to cross the 10ms boundary which is the granularity
  // of the FILETIME, so sleep for 20ms to be on the safe side.
  ::Sleep(20);
  ASSERT_TRUE(file_util::SetLastModifiedTime(crx_path, base::Time::Now()));
  EXPECT_TRUE(cmu::NeedToInstallExtension());
}

TEST(CeeeModuleUtil, GetBrokerProfileNameForIe) {
  MockProcessWinUtils mock_query_admin;

  EXPECT_CALL(mock_query_admin, IsCurrentProcessUacElevated(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(S_OK)));
  const wchar_t* profile_user =
      ceee_module_util::GetBrokerProfileNameForIe();

  EXPECT_CALL(mock_query_admin, IsCurrentProcessUacElevated(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(S_OK)));
  const wchar_t* profile_admin =
      ceee_module_util::GetBrokerProfileNameForIe();

  ASSERT_STRNE(profile_user, profile_admin);
}

TEST(CeeeModuleUtil, GetCromeFrameClientStateKey) {
  EXPECT_EQ(L"Software\\Google\\Update\\ClientState\\"
            L"{8BA986DA-5100-405E-AA35-86F34A02ACBF}",
            ceee_module_util::GetCromeFrameClientStateKey());
}

TEST_F(CeeeModuleUtilTest, GetCollectStatsConsentFromHkcu) {
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());
  base::win::RegKey hkcu(HKEY_CURRENT_USER,
      ceee_module_util::GetCromeFrameClientStateKey().c_str(), KEY_WRITE);

  ASSERT_TRUE(hkcu.WriteValue(google_update::kRegUsageStatsField, kTrue));
  EXPECT_TRUE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hkcu.WriteValue(google_update::kRegUsageStatsField, kFalse));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hkcu.WriteValue(google_update::kRegUsageStatsField, kInvalid));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hkcu.DeleteValue(google_update::kRegUsageStatsField));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());
}

TEST_F(CeeeModuleUtilTest, GetCollectStatsConsentFromHklm) {
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());
  base::win::RegKey hklm(HKEY_LOCAL_MACHINE,
      ceee_module_util::GetCromeFrameClientStateKey().c_str(), KEY_WRITE);

  ASSERT_TRUE(hklm.WriteValue(google_update::kRegUsageStatsField, kTrue));
  EXPECT_TRUE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hklm.WriteValue(google_update::kRegUsageStatsField, kFalse));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hklm.WriteValue(google_update::kRegUsageStatsField, kInvalid));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hklm.DeleteValue(google_update::kRegUsageStatsField));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());
}

TEST_F(CeeeModuleUtilTest, GetCollectStatsConsentHkcuBeforeHklm) {
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());
  base::win::RegKey hkcu(HKEY_CURRENT_USER,
      ceee_module_util::GetCromeFrameClientStateKey().c_str(), KEY_WRITE);

  base::win::RegKey hklm(HKEY_LOCAL_MACHINE,
      ceee_module_util::GetCromeFrameClientStateKey().c_str(), KEY_WRITE);

  ASSERT_TRUE(hklm.WriteValue(google_update::kRegUsageStatsField, kTrue));
  ASSERT_TRUE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hkcu.WriteValue(google_update::kRegUsageStatsField, kFalse));
  EXPECT_FALSE(ceee_module_util::GetCollectStatsConsent());

  ASSERT_TRUE(hkcu.DeleteValue(google_update::kRegUsageStatsField));
  ASSERT_TRUE(hklm.DeleteValue(google_update::kRegUsageStatsField));
}

}  // namespace
