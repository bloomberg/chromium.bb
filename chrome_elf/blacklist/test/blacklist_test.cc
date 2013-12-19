// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "chrome_elf/blacklist/test/blacklist_test_main_dll.h"
#include "testing/gtest/include/gtest/gtest.h"

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
__declspec(dllimport) void TestDll_AddDllToBlacklist(const wchar_t* dll_name);
__declspec(dllimport) void TestDll_RemoveDllFromBlacklist(
    const wchar_t* dll_name);
}

class BlacklistTest : public testing::Test {
  virtual void SetUp() {
    // Force an import from blacklist_test_main_dll.
    InitBlacklistTestDll();

    // Ensure that the beacon state starts off cleared.
    blacklist::ClearBeacon();
  }

  virtual void TearDown() {
    TestDll_RemoveDllFromBlacklist(kTestDllName1);
    TestDll_RemoveDllFromBlacklist(kTestDllName2);
  }
};

TEST_F(BlacklistTest, Beacon) {
  registry_util::RegistryOverrideManager override_manager;
  override_manager.OverrideRegistry(HKEY_CURRENT_USER, L"beacon_test");

  // First call should succeed as the beacon is newly created.
  EXPECT_TRUE(blacklist::CreateBeacon());

  // Second call should fail indicating the beacon already existed.
  EXPECT_FALSE(blacklist::CreateBeacon());

  // First call should find the beacon and delete it.
  EXPECT_TRUE(blacklist::ClearBeacon());

  // Second call should fail to find the beacon and delete it.
  EXPECT_FALSE(blacklist::ClearBeacon());
}

TEST_F(BlacklistTest, AddAndRemoveModules) {
  EXPECT_TRUE(blacklist::AddDllToBlacklist(L"foo.dll"));
  // Adding the same item twice should be idempotent.
  EXPECT_TRUE(blacklist::AddDllToBlacklist(L"foo.dll"));
  EXPECT_TRUE(blacklist::RemoveDllFromBlacklist(L"foo.dll"));
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(L"foo.dll"));

  std::vector<string16> added_dlls;
  added_dlls.reserve(blacklist::kTroublesomeDllsMaxCount);
  for (int i = 0; i < blacklist::kTroublesomeDllsMaxCount; ++i) {
    added_dlls.push_back(base::IntToString16(i) + L".dll");
    EXPECT_TRUE(blacklist::AddDllToBlacklist(added_dlls[i].c_str())) << i;
  }
  EXPECT_FALSE(blacklist::AddDllToBlacklist(L"overflow.dll"));
  for (int i = 0; i < blacklist::kTroublesomeDllsMaxCount; ++i) {
    EXPECT_TRUE(blacklist::RemoveDllFromBlacklist(added_dlls[i].c_str())) << i;
  }
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(L"0.dll"));
  EXPECT_FALSE(blacklist::RemoveDllFromBlacklist(L"63.dll"));
}

TEST_F(BlacklistTest, LoadBlacklistedLibrary) {
// TODO(robertshield): Add 64-bit support.
#if !defined(_WIN64)
  base::FilePath current_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &current_dir));

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
    TestDll_AddDllToBlacklist(test_data[i].dll_name);
    base::ScopedNativeLibrary dll_blacklisted(
        current_dir.Append(test_data[i].dll_name));
    EXPECT_FALSE(dll_blacklisted.is_valid());
    EXPECT_EQ(0u, ::GetEnvironmentVariable(test_data[i].dll_beacon, NULL, 0));
    dll_blacklisted.Reset(NULL);

    // Remove the DLL from the blacklist. Ensure that it loads and that its
    // entry point was called.
    TestDll_RemoveDllFromBlacklist(test_data[i].dll_name);
    base::ScopedNativeLibrary dll(current_dir.Append(test_data[i].dll_name));
    EXPECT_TRUE(dll.is_valid());
    EXPECT_NE(0u, ::GetEnvironmentVariable(test_data[i].dll_beacon, NULL, 0));
    dll.Reset(NULL);
  }
#endif
}
