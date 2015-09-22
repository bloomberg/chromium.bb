// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/sandboxed_zip_analyzer.h"

#include <stdint.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/safe_browsing/zip_analyzer_results.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_utils.h"
#include "crypto/sha2.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

class SandboxedZipAnalyzerTest : public ::testing::Test {
 protected:
  // Constants for validating the data reported by the analyzer.
  struct BinaryData {
    const char* file_basename;
    ClientDownloadRequest_DownloadType download_type;
    const uint8_t* sha256_digest;
    int64_t length;
    bool is_signed;
  };

  // A helper that provides a SandboxedZipAnalyzer::ResultCallback that will
  // store a copy of an analyzer's results and then run a closure.
  class ResultsGetter {
   public:
    ResultsGetter(const base::Closure& quit_closure,
                  zip_analyzer::Results* results)
        : quit_closure_(quit_closure),
          results_(results) {
      DCHECK(results);
      results->success = false;
    }

    SandboxedZipAnalyzer::ResultCallback GetCallback() {
      return base::Bind(&ResultsGetter::OnZipAnalyzerResults,
                        base::Unretained(this));
    }

   private:
    void OnZipAnalyzerResults(const zip_analyzer::Results& results) {
      *results_ = results;
      quit_closure_.Run();
    }

    base::Closure quit_closure_;
    zip_analyzer::Results* results_;
    DISALLOW_COPY_AND_ASSIGN(ResultsGetter);
  };

  SandboxedZipAnalyzerTest()
      : browser_thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP) {}

  void SetUp() override {
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &dir_test_data_));
    dir_test_data_ = dir_test_data_.AppendASCII("safe_browsing");
    dir_test_data_ = dir_test_data_.AppendASCII("download_protection");
  }

  // Runs a sandboxed zip analyzer on |file_path|, writing its results into
  // |results|.
  void RunAnalyzer(const base::FilePath& file_path,
                   zip_analyzer::Results* results) {
    DCHECK(results);
    base::RunLoop run_loop;
    ResultsGetter results_getter(run_loop.QuitClosure(), results);
    scoped_refptr<SandboxedZipAnalyzer> analyzer(
        new SandboxedZipAnalyzer(file_path, results_getter.GetCallback()));
    analyzer->Start();
    run_loop.Run();
  }

#if defined(OS_WIN)
  void ExpectPEHeaders(const BinaryData& data,
                       const ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_EQ(data.is_signed, binary.has_signature());
    if (data.is_signed) {
      ASSERT_LT(0, binary.signature().signed_data_size());
      ASSERT_NE(0U, binary.signature().signed_data(0).size());
    }
    ASSERT_TRUE(binary.has_image_headers());
    ASSERT_TRUE(binary.image_headers().has_pe_headers());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_dos_header());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_file_header());
    EXPECT_TRUE(binary.image_headers().pe_headers().has_optional_headers32());
    EXPECT_FALSE(binary.image_headers().pe_headers().has_optional_headers64());
  }
#endif

  // Verifies expectations about a binary found by the analyzer.
  void ExpectBinary(const BinaryData& data,
                    const ClientDownloadRequest_ArchivedBinary& binary) {
    ASSERT_TRUE(binary.has_file_basename());
    EXPECT_EQ(data.file_basename, binary.file_basename());
    ASSERT_TRUE(binary.has_download_type());
    EXPECT_EQ(data.download_type, binary.download_type());
    ASSERT_TRUE(binary.has_digests());
    ASSERT_TRUE(binary.digests().has_sha256());
    EXPECT_EQ(std::string(data.sha256_digest,
                          data.sha256_digest + crypto::kSHA256Length),
              binary.digests().sha256());
    EXPECT_FALSE(binary.digests().has_sha1());
    EXPECT_FALSE(binary.digests().has_md5());
    ASSERT_TRUE(binary.has_length());
    EXPECT_EQ(data.length, binary.length());
#if defined(OS_WIN)
    // ExtractImageFeatures only implemented for Windows, and only works on PE
    // files.
    if (binary.file_basename().find(".exe") != std::string::npos) {
      ExpectPEHeaders(data, binary);
      return;
    }
#endif  // OS_WIN
    ASSERT_FALSE(binary.has_signature());
    ASSERT_FALSE(binary.has_image_headers());
  }

  static const uint8_t kUnsignedDigest[];
  static const uint8_t kSignedDigest[];
  static const uint8_t kJSEFileDigest[];
  static const BinaryData kUnsignedExe;
  static const BinaryData kSignedExe;
  static const BinaryData kJSEFile;

  base::FilePath dir_test_data_;
  content::TestBrowserThreadBundle browser_thread_bundle_;
  content::InProcessUtilityThreadHelper utility_thread_helper_;
};

// static
const uint8_t SandboxedZipAnalyzerTest::kUnsignedDigest[] = {
    0x1e, 0x95, 0x4d, 0x9c, 0xe0, 0x38, 0x9e, 0x2b, 0xa7, 0x44, 0x72,
    0x16, 0xf2, 0x17, 0x61, 0xf9, 0x8d, 0x1e, 0x65, 0x40, 0xc2, 0xab,
    0xec, 0xdb, 0xec, 0xff, 0x57, 0x0e, 0x36, 0xc4, 0x93, 0xdb};
const uint8_t SandboxedZipAnalyzerTest::kSignedDigest[] = {
    0xe1, 0x1f, 0xfa, 0x0c, 0x9f, 0x25, 0x23, 0x44, 0x53, 0xa9, 0xed,
    0xd1, 0xcb, 0x25, 0x1d, 0x46, 0x10, 0x7f, 0x34, 0xb5, 0x36, 0xad,
    0x74, 0x64, 0x2a, 0x85, 0x84, 0xac, 0xa8, 0xc1, 0xa8, 0xce};
const uint8_t SandboxedZipAnalyzerTest::kJSEFileDigest[] = {
    0x58, 0x91, 0xb5, 0xb5, 0x22, 0xd5, 0xdf, 0x08, 0x6d, 0x0f, 0xf0,
    0xb1, 0x10, 0xfb, 0xd9, 0xd2, 0x1b, 0xb4, 0xfc, 0x71, 0x63, 0xaf,
    0x34, 0xd0, 0x82, 0x86, 0xa2, 0xe8, 0x46, 0xf6, 0xbe, 0x03};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kUnsignedExe = {
  "unsigned.exe",
  ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
  &kUnsignedDigest[0],
  36864,
  false,  // !is_signed
};
const SandboxedZipAnalyzerTest::BinaryData
    SandboxedZipAnalyzerTest::kSignedExe = {
  "signed.exe",
  ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
  &kSignedDigest[0],
  37768,
  true,  // is_signed
};
const SandboxedZipAnalyzerTest::BinaryData SandboxedZipAnalyzerTest::kJSEFile =
    {
        "hello.jse",
        ClientDownloadRequest_DownloadType_WIN_EXECUTABLE,
        &kJSEFileDigest[0],
        6,
        false,  // is_signed
};

TEST_F(SandboxedZipAnalyzerTest, NoBinaries) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_no_binaries.zip"), &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  EXPECT_EQ(0, results.archived_binary.size());
}

TEST_F(SandboxedZipAnalyzerTest, OneUnsignedBinary) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_one_unsigned_binary.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kUnsignedExe, results.archived_binary.Get(0));
}

TEST_F(SandboxedZipAnalyzerTest, TwoBinariesOneSigned) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_two_binaries_one_signed.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(2, results.archived_binary.size());
  ExpectBinary(kUnsignedExe, results.archived_binary.Get(0));
  ExpectBinary(kSignedExe, results.archived_binary.Get(1));
}

TEST_F(SandboxedZipAnalyzerTest, ZippedArchiveNoBinaries) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_archive_no_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  EXPECT_EQ(0, results.archived_binary.size());
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("hello.zip"),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedRarArchiveNoBinaries) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_rar_archive_no_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_FALSE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  EXPECT_EQ(0, results.archived_binary.size());
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("hello.rar"),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedArchiveAndBinaries) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_archive_and_binaries.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kSignedExe, results.archived_binary.Get(0));
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("hello.7z"),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest,
       ZippedArchiveAndBinariesWithTrailingSpaceAndPeriodChars) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_two_binaries_one_archive_"
                                         "trailing_space_and_period_chars.zip"),
              &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_TRUE(results.has_archive);
  ASSERT_EQ(2, results.archived_binary.size());

  BinaryData SignedExe = kSignedExe;
  SignedExe.file_basename = "signed.exe ";
  BinaryData UnsignedExe = kUnsignedExe;
  UnsignedExe.file_basename = "unsigned.exe.";
  ExpectBinary(SignedExe, results.archived_binary.Get(0));
  ExpectBinary(UnsignedExe, results.archived_binary.Get(1));
  ASSERT_EQ(1u, results.archived_archive_filenames.size());
  EXPECT_EQ(FILE_PATH_LITERAL("zipfile_no_binaries.zip  .  . "),
            results.archived_archive_filenames[0].value());
}

TEST_F(SandboxedZipAnalyzerTest, ZippedJSEFile) {
  zip_analyzer::Results results;
  RunAnalyzer(dir_test_data_.AppendASCII("zipfile_one_jse_file.zip"), &results);
  ASSERT_TRUE(results.success);
  EXPECT_TRUE(results.has_executable);
  EXPECT_FALSE(results.has_archive);
  ASSERT_EQ(1, results.archived_binary.size());
  ExpectBinary(kJSEFile, results.archived_binary.Get(0));
  EXPECT_TRUE(results.archived_archive_filenames.empty());
}

}  // namespace safe_browsing
