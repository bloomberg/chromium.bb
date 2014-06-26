// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/environment_data_collection_win.h"

#include <string>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "net/base/winsock_init.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

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
  std::string lsp_path = "c:\\windows\\system32\\mswsock.dll";
  int base_address = 0x77770000;
  int length = 0x180000;

  // Look for dlls that are registered as a LSP.
  safe_browsing::RecordLspFeature(&process_report);

  // Look through dll entries and check that none contains the LSP feature.
  bool lsp_feature_found = false;
  for (int i = 0; i < process_report.dll_size(); ++i) {
    if (process_report.dll(i).path() == lsp_path) {
      // Look for ClientIncidentReport_EnvironmentData_Process_DLL_Feature_LSP
      // through the features of each dll.
      for (int j = 0; j < process_report.dll(i).feature_size(); ++j) {
        if (process_report.dll(i).feature(j) ==
            safe_browsing::ClientIncidentReport_EnvironmentData_Process_Dll::
                LSP)
          lsp_feature_found = true;
      }
    }
  }

  ASSERT_FALSE(lsp_feature_found);

  // Manually add an entry to the process report that will get marked as a LSP.
  safe_browsing::ClientIncidentReport_EnvironmentData_Process_Dll* dll =
      process_report.add_dll();
  dll->set_path(lsp_path);
  dll->set_base_address(base_address);
  dll->set_length(length);

  // Look for dlls that are registered as a LSP.
  safe_browsing::RecordLspFeature(&process_report);

  // Look through dll entries and check if the one we added contains the LSP
  // feature.
  lsp_feature_found = false;
  for (int i = 0; i < process_report.dll_size(); ++i) {
    if (process_report.dll(i).path() == lsp_path) {
      // Look for ClientIncidentReport_EnvironmentData_Process_DLL_Feature_LSP
      // through the features of each dll.
      for (int j = 0; j < process_report.dll(i).feature_size(); ++j) {
        if (process_report.dll(i).feature(j) ==
            safe_browsing::ClientIncidentReport_EnvironmentData_Process_Dll::
                LSP)
          lsp_feature_found = true;
      }
    }
  }

  ASSERT_TRUE(lsp_feature_found);
}
