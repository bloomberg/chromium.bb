// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/platform_file.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "chrome/browser/media/webrtc_log_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kTestReportId[] = "123456789";
const char kTestTime[] = "987654321";

class WebRtcLogUploaderTest : public testing::Test {
 public:
  WebRtcLogUploaderTest() {}

  bool VerifyNumberOfLinesAndContentsOfLastLine(int expected_lines) {
    std::string contents;
    int read = base::ReadFileToString(test_list_path_, &contents);
    EXPECT_GT(read, 0);
    if (read <= 0)
      return false;

    // Verify expected number of lines. Since every line should end with '\n',
    // there should be an additional empty line after splitting.
    std::vector<std::string> lines;
    base::SplitString(contents, '\n', &lines);
    EXPECT_EQ(expected_lines + 1, static_cast<int>(lines.size()));
    if (expected_lines + 1 != static_cast<int>(lines.size()))
      return false;
    EXPECT_TRUE(lines[expected_lines].empty());

    // Verify the contents of the last line. The time (line_parts[0]) is the
    // time when the info was written to the file which we don't know, so just
    // verify that it's not empty.
    std::vector<std::string> line_parts;
    base::SplitString(lines[expected_lines - 1], ',', &line_parts);
    EXPECT_EQ(2u, line_parts.size());
    if (2u != line_parts.size())
      return false;
    EXPECT_FALSE(line_parts[0].empty());
    EXPECT_STREQ(kTestReportId, line_parts[1].c_str());

    return true;
  }

  bool AddLinesToTestFile(int number_of_lines) {
    int flags = base::PLATFORM_FILE_OPEN |
                base::PLATFORM_FILE_APPEND;
    base::PlatformFileError error = base::PLATFORM_FILE_OK;
    base::PlatformFile test_list_file =
        base::CreatePlatformFile(test_list_path_, flags, NULL, &error);
    EXPECT_EQ(base::PLATFORM_FILE_OK, error);
    EXPECT_NE(base::kInvalidPlatformFileValue, test_list_file);
    if (base::PLATFORM_FILE_OK != error ||
        base::kInvalidPlatformFileValue == test_list_file) {
      return false;
    }

    for (int i = 0; i < number_of_lines; ++i) {
      EXPECT_EQ(static_cast<int>(sizeof(kTestTime)) - 1,
                base::WritePlatformFileAtCurrentPos(test_list_file,
                                                    kTestTime,
                                                    sizeof(kTestTime) - 1));
      EXPECT_EQ(1, base::WritePlatformFileAtCurrentPos(test_list_file, ",", 1));
      EXPECT_EQ(static_cast<int>(sizeof(kTestReportId)) - 1,
                base::WritePlatformFileAtCurrentPos(test_list_file,
                                                    kTestReportId,
                                                    sizeof(kTestReportId) - 1));
      EXPECT_EQ(1, base::WritePlatformFileAtCurrentPos(test_list_file,
                                                       "\n", 1));
    }
    EXPECT_TRUE(base::ClosePlatformFile(test_list_file));

    return true;
  }

  base::FilePath test_list_path_;
};

TEST_F(WebRtcLogUploaderTest, AddUploadedLogInfoToUploadListFile) {
  // Get a temporary filename. We don't want the file to exist to begin with
  // since that's the normal use case, hence the delete.
  ASSERT_TRUE(file_util::CreateTemporaryFile(&test_list_path_));
  EXPECT_TRUE(base::DeleteFile(test_list_path_, false));
  scoped_ptr<WebRtcLogUploader> webrtc_log_uploader_(
      new WebRtcLogUploader());

  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(test_list_path_,
                                                           kTestReportId);
  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(test_list_path_,
                                                           kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLinesAndContentsOfLastLine(2));

  const int expected_line_limit = 50;
  ASSERT_TRUE(AddLinesToTestFile(expected_line_limit - 2));
  ASSERT_TRUE(VerifyNumberOfLinesAndContentsOfLastLine(expected_line_limit));

  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(test_list_path_,
                                                           kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLinesAndContentsOfLastLine(expected_line_limit));

  ASSERT_TRUE(AddLinesToTestFile(10));
  ASSERT_TRUE(VerifyNumberOfLinesAndContentsOfLastLine(60));

  webrtc_log_uploader_->AddUploadedLogInfoToUploadListFile(test_list_path_,
                                                           kTestReportId);
  ASSERT_TRUE(VerifyNumberOfLinesAndContentsOfLastLine(expected_line_limit));
}
