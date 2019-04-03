// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <wininet.h>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_file_util.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"
#include "components/download/quarantine/quarantine.h"
#include "components/download/quarantine/quarantine_features_win.h"
#include "net/base/filename_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace download {

namespace {

const char kDummySourceUrl[] = "https://example.com/foo";
const char kDummyReferrerUrl[] = "https://example.com/referrer";
const char kDummyClientGuid[] = "A1B69307-8FA2-4B6F-9181-EA06051A48A7";

const char kMotwForInternetZone[] = "[ZoneTransfer]\r\nZoneId=3\r\n";

const char* const kUntrustedURLs[] = {
    "http://example.com/foo",
    "https://example.com/foo",
    "ftp://example.com/foo",
    "ftp://example.com:2121/foo",
    "data:text/plain,Hello%20world",
    "blob://example.com/126278b3-58f3-4b4a-a914-1d1185d634f6",
    "about:internet",
    ""};

// Creates a non-empty file at |file_path|.
bool CreateFile(const base::FilePath& file_path) {
  constexpr char kTestData[] = "Hello world!";

  return base::WriteFile(file_path, kTestData, base::size(kTestData)) ==
         static_cast<int>(base::size(kTestData));
}

// Reads the Zone.Identifier alternate data stream from |file_path| into
// |contents|.
bool GetZoneIdentifierStreamContents(const base::FilePath& file_path,
                                     std::string* contents) {
  DCHECK(contents);

  const base::FilePath::CharType kMotwStreamSuffix[] =
      FILE_PATH_LITERAL(":Zone.Identifier");

  base::FilePath zone_identifier_stream_path(file_path.value() +
                                             kMotwStreamSuffix);

  return base::ReadFileToString(zone_identifier_stream_path, contents);
}

}  // namespace

class QuarantineWinTest : public ::testing::Test {
 public:
  QuarantineWinTest() = default;
  ~QuarantineWinTest() override = default;

  void SetUp() override { ASSERT_TRUE(scoped_temp_dir_.CreateUniqueTempDir()); }

  base::FilePath GetTempDir() { return scoped_temp_dir_.GetPath(); }

 private:
  base::ScopedTempDir scoped_temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(QuarantineWinTest);
};

// If the file is missing, the QuarantineFile() call should return FILE_MISSING.
TEST_F(QuarantineWinTest, MissingFile) {
  EXPECT_EQ(QuarantineFileResult::FILE_MISSING,
            QuarantineFile(GetTempDir().AppendASCII("does-not-exist.exe"),
                           GURL(kDummySourceUrl), GURL(kDummyReferrerUrl),
                           kDummyClientGuid));
}

// On Windows systems, files downloaded from a local source are considered
// trustworthy. Hence they aren't annotated with source information. This test
// verifies this behavior since the other tests in this suite would pass with a
// false positive if local files are being annotated with the MOTW for the
// internet zone.
TEST_F(QuarantineWinTest, LocalFile_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  const char* const kLocalSourceURLs[] = {"http://localhost/foo",
                                          "file:///C:/some-local-dir/foo.exe"};

  for (const char* source_url : kLocalSourceURLs) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << source_url);

    ASSERT_TRUE(CreateFile(test_file));

    EXPECT_EQ(
        QuarantineFileResult::OK,
        QuarantineFile(test_file, GURL(source_url), GURL(), kDummyClientGuid));

    std::string zone_identifier;
    GetZoneIdentifierStreamContents(test_file, &zone_identifier);

    // No zone identifier for local source.
    EXPECT_TRUE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
}

// A file downloaded from the internet should be annotated with .. something.
// The specific zone assigned to our dummy source URL depends on the local
// configuration. But no sane configuration should be treating the dummy URL as
// a trusted source for anything.
TEST_F(QuarantineWinTest, DownloadedFile_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  for (const char* source_url : kUntrustedURLs) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << source_url);

    ASSERT_TRUE(CreateFile(test_file));
    EXPECT_EQ(
        QuarantineFileResult::OK,
        QuarantineFile(test_file, GURL(source_url), GURL(), kDummyClientGuid));

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
}

TEST_F(QuarantineWinTest, UnsafeReferrer_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");

  std::vector<std::string> unsafe_referrers(std::begin(kUntrustedURLs),
                                            std::end(kUntrustedURLs));

  // Add one more test case.
  std::string huge_referrer = "http://example.com/";
  huge_referrer.append(INTERNET_MAX_URL_LENGTH * 2, 'a');
  unsafe_referrers.push_back(huge_referrer);

  for (const auto referrer_url : unsafe_referrers) {
    SCOPED_TRACE(::testing::Message() << "Trying URL " << referrer_url);

    ASSERT_TRUE(CreateFile(test_file));
    EXPECT_EQ(QuarantineFileResult::OK,
              QuarantineFile(test_file, GURL("http://example.com/good"),
                             GURL(referrer_url), kDummyClientGuid));

    std::string zone_identifier;
    ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

    // The actual assigned zone could be anything and the contents of the zone
    // identifier depends on the version of Windows. So only testing that there
    // is a zone annotation.
    EXPECT_FALSE(zone_identifier.empty());

    ASSERT_TRUE(base::DeleteFile(test_file, false));
  }
}

// An empty source URL should result in a file that's treated the same as one
// downloaded from the internet.
TEST_F(QuarantineWinTest, EmptySource_DependsOnLocalConfig) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(), GURL(), kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  // The actual assigned zone could be anything and the contents of the zone
  // identifier depends on the version of Windows. So only testing that there is
  // a zone annotation.
  EXPECT_FALSE(zone_identifier.empty());
}

// Empty files aren't passed to AVScanFile. They are instead marked manually. If
// the file is passed to AVScanFile, then there wouldn't be a MOTW attached to
// it and the test would fail.
TEST_F(QuarantineWinTest, EmptyFile) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_EQ(0, base::WriteFile(test_file, "", 0u));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_STREQ(zone_identifier.c_str(), kMotwForInternetZone);
}

// If there is no client GUID supplied to the QuarantineFile() call, then rather
// than invoking AVScanFile, the MOTW will be applied manually.  If the file is
// passed to AVScanFile, then there wouldn't be a MOTW attached to it and the
// test would fail.
TEST_F(QuarantineWinTest, NoClientGuid) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, net::FilePathToFileURL(test_file), GURL(),
                           std::string()));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_STREQ(zone_identifier.c_str(), kMotwForInternetZone);
}

// URLs longer than INTERNET_MAX_URL_LENGTH are known to break URLMon. Such a
// URL, when used as a source URL shouldn't break QuarantineFile() which should
// mark the file as being from the internet zone as a safe fallback.
TEST_F(QuarantineWinTest, SuperLongURL) {
  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  std::string source_url("http://example.com/");
  source_url.append(INTERNET_MAX_URL_LENGTH * 2, 'a');
  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(source_url), GURL(), std::string()));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_STREQ(zone_identifier.c_str(), kMotwForInternetZone);
}

// On domain-joined machines, the IAttachmentExecute code path is taken, and the
// output depends on the Windows version.
TEST_F(QuarantineWinTest, EnterpriseUserZoneIdentifier) {
  base::win::ScopedDomainStateForTesting scoped_domain(true);

  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kDummySourceUrl),
                           GURL(kDummyReferrerUrl), kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  std::string expected = kMotwForInternetZone;
  // On Win10, the MotW now contains the HostUrl and ReferrerUrl values.
  if (base::win::GetVersion() >= base::win::VERSION_WIN10) {
    expected.append(base::StringPrintf("ReferrerUrl=%s\r\nHostUrl=%s\r\n",
                                       kDummyReferrerUrl, kDummySourceUrl));
  }

  EXPECT_EQ(zone_identifier, expected);
}

// When the InvokeAttachmentServices is disabled, the fallback code path that
// manually sets the MotW is always invoked. The original fallback code only
// sets the ZoneId value.
TEST_F(QuarantineWinTest, DisableInvokeAttachmentServices) {
  base::win::ScopedDomainStateForTesting scoped_domain(false);

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      // Enabled features.
      {},
      // Disabled features.
      {kInvokeAttachmentServices});

  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kDummySourceUrl),
                           GURL(kDummyReferrerUrl), kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  EXPECT_STREQ(zone_identifier.c_str(), kMotwForInternetZone);
}

// Tests the expected MotW when the AugmentedZoneIdentifier feature is enabled.
TEST_F(QuarantineWinTest, AugmentedZoneIdentifier) {
  base::win::ScopedDomainStateForTesting scoped_domain(false);

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      // Enabled features.
      {kAugmentedZoneIdentifier},
      // Disabled features.
      {kInvokeAttachmentServices});

  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kDummySourceUrl),
                           GURL(kDummyReferrerUrl), kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  std::string expected = kMotwForInternetZone;
  expected.append(base::StringPrintf("ReferrerUrl=%s\r\nHostUrl=%s\r\n",
                                     kDummyReferrerUrl, kDummySourceUrl));
  EXPECT_EQ(zone_identifier, expected);
}

// Tests the expected MotW when the AugmentedZoneIdentifier feature is enabled
// and no referrer is provided to the QuarantineFile() function.
TEST_F(QuarantineWinTest, AugmentedZoneIdentifierNoReferrer) {
  base::win::ScopedDomainStateForTesting scoped_domain(false);

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      // Enabled features.
      {kAugmentedZoneIdentifier},
      // Disabled features.
      {kInvokeAttachmentServices});

  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(kDummySourceUrl), GURL(),
                           kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  std::string expected = kMotwForInternetZone;
  expected.append(base::StringPrintf("HostUrl=%s\r\n", kDummySourceUrl));

  EXPECT_EQ(zone_identifier, expected);
}

// Tests the expected MotW when the AugmentedZoneIdentifier feature is enabled
// and no source is provided to the QuarantineFile() function.
TEST_F(QuarantineWinTest, AugmentedZoneIdentifierNoSource) {
  base::win::ScopedDomainStateForTesting scoped_domain(false);

  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      // Enabled features.
      {kAugmentedZoneIdentifier},
      // Disabled features.
      {kInvokeAttachmentServices});

  base::FilePath test_file = GetTempDir().AppendASCII("foo.exe");
  ASSERT_TRUE(CreateFile(test_file));

  EXPECT_EQ(QuarantineFileResult::OK,
            QuarantineFile(test_file, GURL(), GURL(kDummyReferrerUrl),
                           kDummyClientGuid));

  std::string zone_identifier;
  ASSERT_TRUE(GetZoneIdentifierStreamContents(test_file, &zone_identifier));

  std::string expected = kMotwForInternetZone;
  expected.append(base::StringPrintf("ReferrerUrl=%s\r\nHostUrl=%s\r\n",
                                     kDummyReferrerUrl, "about:internet"));

  EXPECT_EQ(zone_identifier, expected);
}

}  // namespace download
