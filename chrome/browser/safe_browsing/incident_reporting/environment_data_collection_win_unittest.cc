// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/environment_data_collection_win.h"

#include <string>

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_reg_util_win.h"
#include "base/win/registry.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_unittest_util_win.h"
#include "chrome/browser/safe_browsing/incident_reporting/module_integrity_verifier_win.h"
#include "chrome/browser/safe_browsing/path_sanitizer.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "chrome_elf/chrome_elf_constants.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const wchar_t test_dll[] = L"test_name.dll";

// Helper function that returns true if a dll with filename |dll_name| is
// found in |process_report|.
bool ProcessReportContainsDll(
    const safe_browsing::ClientIncidentReport_EnvironmentData_Process&
        process_report,
    const base::FilePath& dll_name) {
  for (int i = 0; i < process_report.dll_size(); ++i) {
    base::FilePath current_dll =
        base::FilePath::FromUTF8Unsafe(process_report.dll(i).path());

    if (current_dll.BaseName() == dll_name)
      return true;
  }

  return false;
}

// Look through dll entries and check for the presence of the LSP feature for
// |dll|.
bool DllEntryContainsLspFeature(
    const safe_browsing::ClientIncidentReport_EnvironmentData_Process&
        process_report,
    const std::string& dll) {
  for (int i = 0; i < process_report.dll_size(); ++i) {
    if (process_report.dll(i).path() == dll) {
      // Verify each feature of |dll|.
      for (int j = 0; j < process_report.dll(i).feature_size(); ++j) {
        if (process_report.dll(i).feature(j) ==
            safe_browsing::ClientIncidentReport_EnvironmentData_Process_Dll::
                LSP)
          // LSP feature found.
          return true;
      }
    }
  }

  return false;
}

}  // namespace

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, CollectDlls) {
  // This test will check if the CollectDlls method works by loading
  // a dll and then checking if we can find it within the process report.
  // Pick msvidc32.dll as it is present from WinXP to Win8 and yet rarely used.
  // msvidc32.dll exists in both 32 and 64 bit versions.
  base::FilePath msvdc32_dll(L"msvidc32.dll");

  safe_browsing::ClientIncidentReport_EnvironmentData_Process process_report;
  safe_browsing::CollectDlls(&process_report);

  ASSERT_FALSE(ProcessReportContainsDll(process_report, msvdc32_dll));

  // Redo the same verification after loading a new dll.
  base::ScopedNativeLibrary library(msvdc32_dll);

  process_report.clear_dll();
  safe_browsing::CollectDlls(&process_report);

  ASSERT_TRUE(ProcessReportContainsDll(process_report, msvdc32_dll));
}

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, RecordLspFeature) {
  net::EnsureWinsockInit();

  // Populate our incident report with loaded modules.
  safe_browsing::ClientIncidentReport_EnvironmentData_Process process_report;
  safe_browsing::CollectDlls(&process_report);

  // We'll test RecordLspFeatures against a real dll registered as a LSP. All
  // dll paths are expected to be lowercase in the process report.
  std::string lsp = "c:\\windows\\system32\\mswsock.dll";
  int base_address = 0x77770000;
  int length = 0x180000;

  safe_browsing::RecordLspFeature(&process_report);

  // Return successfully if LSP feature is found.
  if (DllEntryContainsLspFeature(process_report, lsp))
    return;

  // |lsp| was not already loaded into the current process. Manually add it
  // to the process report so that it will get marked as a LSP.
  safe_browsing::ClientIncidentReport_EnvironmentData_Process_Dll* dll =
      process_report.add_dll();
  dll->set_path(lsp);
  dll->set_base_address(base_address);
  dll->set_length(length);

  safe_browsing::RecordLspFeature(&process_report);

  // Return successfully if LSP feature is found.
  if (DllEntryContainsLspFeature(process_report, lsp))
    return;

  FAIL() << "No LSP feature found for " << lsp;
}

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, CollectDllBlacklistData) {
  // Ensure that CollectDllBlacklistData correctly adds the set of sanitized dll
  // names currently stored in the registry to the report.
  registry_util::RegistryOverrideManager override_manager;
  override_manager.OverrideRegistry(HKEY_CURRENT_USER);

  base::win::RegKey blacklist_registry_key(HKEY_CURRENT_USER,
                                           blacklist::kRegistryFinchListPath,
                                           KEY_QUERY_VALUE | KEY_SET_VALUE);

  // Check that with an empty registry the blacklisted dlls field is left empty.
  safe_browsing::ClientIncidentReport_EnvironmentData_Process process_report;
  safe_browsing::CollectDllBlacklistData(&process_report);
  EXPECT_EQ(0, process_report.blacklisted_dll_size());

  // Check that after adding exactly one dll to the registry it appears in the
  // process report.
  blacklist_registry_key.WriteValue(test_dll, test_dll);
  safe_browsing::CollectDllBlacklistData(&process_report);
  ASSERT_EQ(1, process_report.blacklisted_dll_size());

  base::string16 process_report_dll =
      base::UTF8ToWide(process_report.blacklisted_dll(0));
  EXPECT_EQ(base::string16(test_dll), process_report_dll);

  // Check that if the registry contains the full path to a dll it is properly
  // sanitized before being reported.
  blacklist_registry_key.DeleteValue(test_dll);
  process_report.clear_blacklisted_dll();

  base::FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_HOME, &path));
  base::string16 input_path =
      path.Append(FILE_PATH_LITERAL("test_path.dll")).value();

  std::string path_expected = base::FilePath(FILE_PATH_LITERAL("~"))
                                  .Append(FILE_PATH_LITERAL("test_path.dll"))
                                  .AsUTF8Unsafe();

  blacklist_registry_key.WriteValue(input_path.c_str(), input_path.c_str());
  safe_browsing::CollectDllBlacklistData(&process_report);

  ASSERT_EQ(1, process_report.blacklisted_dll_size());
  std::string process_report_path = process_report.blacklisted_dll(0);
  EXPECT_EQ(path_expected, process_report_path);
}

TEST(SafeBrowsingEnvironmentDataCollectionWinTest, VerifyLoadedModules) {
  //  Load the test modules.
  std::vector<base::ScopedNativeLibrary> test_dlls(
      safe_browsing::kTestDllNamesCount);
  for (size_t i = 0; i < safe_browsing::kTestDllNamesCount; ++i) {
    test_dlls[i].Reset(LoadNativeLibrary(
        base::FilePath(safe_browsing::kTestDllNames[i]), NULL));
  }

  // Edit the first byte of the function exported by the first module. Calling
  // GetModuleHandle so we do not increment the library ref count.
  HMODULE module_handle = GetModuleHandle(safe_browsing::kTestDllNames[0]);
  EXPECT_NE(reinterpret_cast<HANDLE>(NULL), module_handle);
  uint8_t* export_addr = reinterpret_cast<uint8_t*>(
      GetProcAddress(module_handle, safe_browsing::kTestExportName));
  EXPECT_NE(reinterpret_cast<uint8_t*>(NULL), export_addr);

  uint8_t new_val = (*export_addr) + 1;
  SIZE_T bytes_written = 0;
  WriteProcessMemory(GetCurrentProcess(),
                     export_addr,
                     reinterpret_cast<void*>(&new_val),
                     1,
                     &bytes_written);
  EXPECT_EQ(1, bytes_written);

  safe_browsing::ClientIncidentReport_EnvironmentData_Process process_report;
  safe_browsing::CollectModuleVerificationData(
      safe_browsing::kTestDllNames,
      safe_browsing::kTestDllNamesCount,
      &process_report);

  // CollectModuleVerificationData should return the single modified module and
  // its modified export.  The other module, being unmodified, is omitted from
  // the returned list of modules.
  EXPECT_EQ(1, process_report.module_state_size());

  EXPECT_EQ(base::WideToUTF8(std::wstring(safe_browsing::kTestDllNames[0])),
            process_report.module_state(0).name());
  EXPECT_EQ(
      safe_browsing::ClientIncidentReport_EnvironmentData_Process_ModuleState::
          MODULE_STATE_MODIFIED,
      process_report.module_state(0).modified_state());
  EXPECT_EQ(1, process_report.module_state(0).modified_export_size());
  EXPECT_EQ(std::string(safe_browsing::kTestExportName),
            process_report.module_state(0).modified_export(0));
}
