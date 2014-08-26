// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/test/scoped_path_override.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "version.h"  // NOLINT

namespace safe_browsing {

namespace {

const wchar_t kChromeDll[] = L"chrome.dll";
const wchar_t kChromeChildDll[] = L"chrome_child.dll";
const wchar_t kChromeElfDll[] = L"chrome_elf.dll";
const wchar_t kChromeExe[] = L"chrome.exe";
const wchar_t kSignedBinaryDll[] = L"signed_binary.dll";

// Helper function to erase the content of a binary to make sure the signature
// verification will fail.
bool EraseFileContent(const base::FilePath& file_path) {
  FILE* file = base::OpenFile(file_path, "w");

  if (file == NULL)
    return false;

  bool success = base::TruncateFile(file);
  return base::CloseFile(file) && success;
}

}  // namespace

class BinaryIntegrityAnalyzerWinTest : public ::testing::Test {
 public:
  BinaryIntegrityAnalyzerWinTest();

  void OnAddIncident(
      scoped_ptr<ClientIncidentReport_IncidentData> incident_data);

 protected:
  bool callback_called_;
  base::FilePath test_data_dir_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<base::ScopedPathOverride> exe_dir_override_;
  scoped_ptr<ClientIncidentReport_IncidentData> incident_data_;
};

BinaryIntegrityAnalyzerWinTest::BinaryIntegrityAnalyzerWinTest()
    : callback_called_(false) {
  temp_dir_.CreateUniqueTempDir();
  base::CreateDirectory(temp_dir_.path().AppendASCII(CHROME_VERSION_STRING));

  // We retrieve DIR_TEST_DATA here because it is based on DIR_EXE and we are
  // about to override the path to the latter.
  if (!PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir_))
    NOTREACHED();

  exe_dir_override_.reset(
      new base::ScopedPathOverride(base::DIR_EXE, temp_dir_.path()));
}

// Mock the AddIncidentCallback so we can test that VerifyBinaryIntegrity
// adds an incident callback when a signature verification fails.
void BinaryIntegrityAnalyzerWinTest::OnAddIncident(
    scoped_ptr<ClientIncidentReport_IncidentData> incident_data) {
  callback_called_ = true;

  // Take ownership of the incident so that the text fixture can inspect it.
  incident_data_ = incident_data.Pass();
}

TEST_F(BinaryIntegrityAnalyzerWinTest, GetCriticalBinariesPath) {
  // Expected paths.
  std::vector<base::FilePath> critical_binaries_path_expected;
  critical_binaries_path_expected.push_back(
      temp_dir_.path().Append(kChromeExe));
  critical_binaries_path_expected.push_back(
      temp_dir_.path().AppendASCII(CHROME_VERSION_STRING).Append(kChromeDll));
  critical_binaries_path_expected.push_back(
      temp_dir_.path().AppendASCII(CHROME_VERSION_STRING).Append(
          kChromeChildDll));
  critical_binaries_path_expected.push_back(
      temp_dir_.path().AppendASCII(CHROME_VERSION_STRING).Append(
          kChromeElfDll));

  std::vector<base::FilePath> critical_binaries_path =
      GetCriticalBinariesPath();

  ASSERT_THAT(critical_binaries_path,
              ::testing::ContainerEq(critical_binaries_path_expected));
}

TEST_F(BinaryIntegrityAnalyzerWinTest, VerifyBinaryIntegrity) {
  // Copy the signed dll to the temp exe directory.
  base::FilePath signed_binary_path(test_data_dir_);
  signed_binary_path =
      signed_binary_path.Append(L"safe_browsing").Append(kSignedBinaryDll);

  base::FilePath chrome_elf_path(temp_dir_.path());
  chrome_elf_path =
      chrome_elf_path.Append(TEXT(CHROME_VERSION_STRING)).Append(kChromeElfDll);

  ASSERT_TRUE(base::CopyFile(signed_binary_path, chrome_elf_path));

  AddIncidentCallback callback = base::Bind(
      &BinaryIntegrityAnalyzerWinTest::OnAddIncident, base::Unretained(this));

  VerifyBinaryIntegrity(callback);
  ASSERT_FALSE(callback_called_);

  ASSERT_TRUE(EraseFileContent(chrome_elf_path));

  VerifyBinaryIntegrity(callback);
  ASSERT_TRUE(callback_called_);

  // Verify that the incident report contains the expected data.
  ASSERT_TRUE(incident_data_->has_binary_integrity());
  ASSERT_TRUE(incident_data_->binary_integrity().has_file_basename());
  ASSERT_EQ("chrome_elf.dll",
            incident_data_->binary_integrity().file_basename());
  ASSERT_TRUE(incident_data_->binary_integrity().has_signature());
}

}  // namespace safe_browsing
