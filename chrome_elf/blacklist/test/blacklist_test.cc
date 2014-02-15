// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/case_conversion.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/blacklist/test/blacklist_test_main_dll.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "version.h"  // NOLINT

const wchar_t kTestDllName1[] = L"blacklist_test_dll_1.dll";
const wchar_t kTestDllName2[] = L"blacklist_test_dll_2.dll";
const wchar_t kTestDllName3[] = L"blacklist_test_dll_3.dll";

const wchar_t kDll2Beacon[] = L"{F70A0100-2889-4629-9B44-610FE5C73231}";
const wchar_t kDll3Beacon[] = L"{9E056AEC-169E-400c-B2D0-5A07E3ACE2EB}";

extern const wchar_t* kEnvVars[];

extern "C" {
// When modifying the blacklist in the test process, use the exported test dll
// functions on the test blacklist dll, not the ones linked into the test
// executable itself.
__declspec(dllimport) bool TestDll_AddDllToBlacklist(const wchar_t* dll_name);
__declspec(dllimport) bool TestDLL_IsBlacklistInitialized();
__declspec(dllimport) bool TestDll_RemoveDllFromBlacklist(
    const wchar_t* dll_name);
}

class BlacklistTest : public testing::Test {
  virtual void SetUp() {
    // Force an import from blacklist_test_main_dll.
    InitBlacklistTestDll();
  }

  virtual void TearDown() {
    TestDll_RemoveDllFromBlacklist(kTestDllName1);
    TestDll_RemoveDllFromBlacklist(kTestDllName2);
    TestDll_RemoveDllFromBlacklist(kTestDllName3);
  }
};

TEST_F(BlacklistTest, Beacon) {
  registry_util::RegistryOverrideManager override_manager;
  override_manager.OverrideRegistry(HKEY_CURRENT_USER, L"beacon_test");

  base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER,
                                           blacklist::kRegistryBeaconPath,
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // Ensure that the beacon state starts off enabled for this version.
  LONG result = blacklist_registry_key.WriteValue(blacklist::kBeaconState,
                                                  blacklist::BLACKLIST_ENABLED);
  EXPECT_EQ(ERROR_SUCCESS, result);

  result = blacklist_registry_key.WriteValue(blacklist::kBeaconVersion,
                                             TEXT(CHROME_VERSION_STRING));
  EXPECT_EQ(ERROR_SUCCESS, result);

  // First call should find the beacon and reset it.
  EXPECT_TRUE(blacklist::ResetBeacon());

  // First call should succeed as the beacon is enabled.
  EXPECT_TRUE(blacklist::LeaveSetupBeacon());

  // Second call should fail indicating the beacon wasn't set as enabled.
  EXPECT_FALSE(blacklist::LeaveSetupBeacon());

  // Resetting the beacon should work when setup beacon is present.
  EXPECT_TRUE(blacklist::ResetBeacon());

  // Change the version and ensure that the setup fails due to the version
  // mismatch.
  base::string16 different_version(L"other_version");
  ASSERT_NE(different_version, TEXT(CHROME_VERSION_STRING));

  result = blacklist_registry_key.WriteValue(blacklist::kBeaconVersion,
                                             different_version.c_str());
  EXPECT_EQ(ERROR_SUCCESS, result);

  EXPECT_FALSE(blacklist::LeaveSetupBeacon());
}

TEST_F(BlacklistTest, AddAndRemoveModules) {
  EXPECT_TRUE(blacklist::AddDllToBlacklist(L"foo.dll"));
  // Adding the same item twice should be idempotent.
  EXPECT_TRUE(blacklist::AddDllToBlacklist(L"foo.dll"));
  EXPECT_TRUE(blacklist::RemoveDllFromBlacklist(L"foo.dll"));
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(L"foo.dll"));

  // Increase the blacklist size by 1 to include the NULL pointer
  // that marks the end.
  int empty_spaces = blacklist::kTroublesomeDllsMaxCount - (
      blacklist::BlacklistSize() + 1);
  std::vector<base::string16> added_dlls;
  added_dlls.reserve(empty_spaces);
  for (int i = 0; i < empty_spaces; ++i) {
    added_dlls.push_back(base::IntToString16(i) + L".dll");
    EXPECT_TRUE(blacklist::AddDllToBlacklist(added_dlls[i].c_str())) << i;
  }
  EXPECT_FALSE(blacklist::AddDllToBlacklist(L"overflow.dll"));
  for (int i = 0; i < empty_spaces; ++i) {
    EXPECT_TRUE(blacklist::RemoveDllFromBlacklist(added_dlls[i].c_str())) << i;
  }
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(added_dlls[0].c_str()));
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(
    added_dlls[empty_spaces - 1].c_str()));
}

TEST_F(BlacklistTest, LoadBlacklistedLibrary) {
  base::FilePath current_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &current_dir));

  // Ensure that the blacklist is loaded.
  ASSERT_TRUE(TestDLL_IsBlacklistInitialized());

  // Test that an un-blacklisted DLL can load correctly.
  base::ScopedNativeLibrary dll1(current_dir.Append(kTestDllName1));
  EXPECT_TRUE(dll1.is_valid());
  dll1.Reset(NULL);

  struct TestData {
    const wchar_t* dll_name;
    const wchar_t* dll_beacon;
  } test_data[] = {
    { kTestDllName2, kDll2Beacon },
    { kTestDllName3, kDll3Beacon }
  };
  for (int i = 0 ; i < arraysize(test_data); ++i) {
    // Add the DLL to the blacklist, ensure that it is not loaded both by
    // inspecting the handle returned by LoadLibrary and by looking for an
    // environment variable that is set when the DLL's entry point is called.
    EXPECT_TRUE(TestDll_AddDllToBlacklist(test_data[i].dll_name));
    base::ScopedNativeLibrary dll_blacklisted(
        current_dir.Append(test_data[i].dll_name));
    EXPECT_FALSE(dll_blacklisted.is_valid());
    EXPECT_EQ(0u, ::GetEnvironmentVariable(test_data[i].dll_beacon, NULL, 0));
    dll_blacklisted.Reset(NULL);

    // Remove the DLL from the blacklist. Ensure that it loads and that its
    // entry point was called.
    EXPECT_TRUE(TestDll_RemoveDllFromBlacklist(test_data[i].dll_name));
    base::ScopedNativeLibrary dll(current_dir.Append(test_data[i].dll_name));
    EXPECT_TRUE(dll.is_valid());
    EXPECT_NE(0u, ::GetEnvironmentVariable(test_data[i].dll_beacon, NULL, 0));
    dll.Reset(NULL);

    ::SetEnvironmentVariable(test_data[i].dll_beacon, NULL);

    // Ensure that the dll won't load even if the name has different
    // capitalization.
    base::string16 uppercase_name = base::i18n::ToUpper(test_data[i].dll_name);
    EXPECT_TRUE(TestDll_AddDllToBlacklist(uppercase_name.c_str()));
    base::ScopedNativeLibrary dll_blacklisted_different_case(
        current_dir.Append(test_data[i].dll_name));
    EXPECT_FALSE(dll_blacklisted_different_case.is_valid());
    EXPECT_EQ(0u, ::GetEnvironmentVariable(test_data[i].dll_beacon, NULL, 0));
    dll_blacklisted_different_case.Reset(NULL);

    EXPECT_TRUE(TestDll_RemoveDllFromBlacklist(uppercase_name.c_str()));
  }
}
