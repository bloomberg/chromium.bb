// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/quarantine.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/histogram_tester.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

const char kDummySourceUrl[] = "https://example.com/foo";
const char kDummyReferrerUrl[] = "https://example.com/referrer";
const char kDummyClientGuid[] = "A1B69307-8FA2-4B6F-9181-EA06051A48A7";

const char kMotwForInternetZone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";
const base::FilePath::CharType kMotwStreamSuffix[] =
    FILE_PATH_LITERAL(":Zone.Identifier");

}  // namespace

// If the file is missing, the QuarantineFile() call should return FILE_MISSING.
TEST(QuarantineWinTest, MissingFile) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());

  EXPECT_EQ(QuarantineFileResult::FILE_MISSING,
            QuarantineFile(test_dir.path().AppendASCII("does-not-exist.exe"),
                           GURL(kDummySourceUrl), GURL(kDummyReferrerUrl),
                           kDummyClientGuid));
}

// On Windows systems, files downloaded from a local source are considered
// trustworthy. Hence they aren't annotated with source information. This test
// verifies this behavior since the other tests in this suite would pass with a
// false positive if local files are being annotated with the MOTW for the
// internet zone.
TEST(QuarantineWinTest, LocalFileZoneAssumption_DependsOnLocalConfig) {
  base::HistogramTester histogram_tester;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file = test_dir.path().AppendASCII("foo.exe");
  ASSERT_EQ(5, base::WriteFile(test_file, "Hello", 5u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           kDummyClientGuid));
  std::string contents;
  EXPECT_FALSE(base::ReadFileToString(
      base::FilePath(test_file.value() + kMotwStreamSuffix), &contents));

  // Bucket 1 is SUCCESS_WITHOUT_MOTW.
  histogram_tester.ExpectUniqueSample("Download.AttachmentServices.Result", 1,
                                      1);
}

// A file downloaded from the internet should be annotated with .. something.
// The specific zone assigned to our dummy source URL depends on the local
// configuration. But no sane configuration should be treating the dummy URL as
// a trusted source for anything.
TEST(QuarantineWinTest, DownloadedFile_DependsOnLocalConfig) {
  base::HistogramTester histogram_tester;
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file = test_dir.path().AppendASCII("foo.exe");
  ASSERT_EQ(5, base::WriteFile(test_file, "Hello", 5u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kDummySourceUrl),
                           GURL(kDummyReferrerUrl), kDummyClientGuid));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(
      base::FilePath(test_file.value() + kMotwStreamSuffix), &contents));
  // The actual assigned zone could be anything. So only testing that there is a
  // zone annotation.
  EXPECT_FALSE(contents.empty());

  // Bucket 0 is SUCCESS_WITH_MOTW.
  histogram_tester.ExpectUniqueSample("Download.AttachmentServices.Result", 0,
                                      1);
}

// Empty files aren't passed to AVScanFile. They are instead marked manually. If
// the file is passed to AVScanFile, then there wouldn't be a MOTW attached to
// it and the test would fail.
TEST(QuarantineWinTest, EmptyFile) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file = test_dir.path().AppendASCII("foo.exe");
  ASSERT_EQ(0, base::WriteFile(test_file, "", 0u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           kDummyClientGuid));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(
      base::FilePath(test_file.value() + kMotwStreamSuffix), &contents));
  EXPECT_STREQ(kMotwForInternetZone, contents.c_str());
}

// If there is no client GUID supplied to the QuarantineFile() call, then rather
// than invoking AVScanFile, the MOTW will be applied manually.  If the file is
// passed to AVScanFile, then there wouldn't be a MOTW attached to it and the
// test would fail.
TEST(QuarantineWinTest, NoClientGuid) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());
  base::FilePath test_file = test_dir.path().AppendASCII("foo.exe");
  ASSERT_EQ(5, base::WriteFile(test_file, "Hello", 5u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           std::string()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(
      base::FilePath(test_file.value() + kMotwStreamSuffix), &contents));
  EXPECT_STREQ(kMotwForInternetZone, contents.c_str());
}

}  // content
