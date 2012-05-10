// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The tests in this file are chromeos only.

#include "chrome/browser/ui/webui/feedback_ui.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

// This macro helps avoid wrapped lines in the test structs.
#define FPL(x) FILE_PATH_LITERAL(x)

namespace {

// Simple function to create a file with |filename|.
void CreateFile(const FilePath& filename) {
  FILE* fp = file_util::OpenFile(filename, "w");
  ASSERT_TRUE(fp != NULL);
  file_util::CloseFile(fp);
}

std::string GetScreenshotFilename(const std::string timestamp) {
  return std::string("Screenshot_") + timestamp + std::string(".png");
}

std::string GetScreenshotUrl(const std::string timestamp) {
  return std::string("chrome://screenshots/saved/") +
      GetScreenshotFilename(timestamp);
}

class FeedbackUITest : public testing::Test {
 public:
  FeedbackUITest() {}
  ~FeedbackUITest() {}
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }
 protected:
  void CreateScreenshotFile(const std::string& timestamp) {
    FilePath filepath = temp_dir_.path().Append(
        FILE_PATH_LITERAL(GetScreenshotFilename(timestamp)));
    ASSERT_NO_FATAL_FAILURE(CreateFile(filepath));
  }

  ScopedTempDir temp_dir_;
  std::vector<std::string> saved_screenshots_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FeedbackUITest);
};

TEST_F(FeedbackUITest, GetMostRecentScreenshotsNoScreenShot) {
  // Create a random file.
  FilePath filepath = temp_dir_.path().Append(FILE_PATH_LITERAL("garbage.png"));
  ASSERT_NO_FATAL_FAILURE(CreateFile(filepath));
  // Expect getting no screenshot.
  FeedbackUI::GetMostRecentScreenshots(
      temp_dir_.path(), &saved_screenshots_, 2);
  ASSERT_TRUE(saved_screenshots_.empty());
}

TEST_F(FeedbackUITest, GetMostRecentScreenshotsOneScreenShot) {
  // Create 1 screenshot.
  ASSERT_NO_FATAL_FAILURE(CreateScreenshotFile("20120416-152908"));
  // Expect getting 1 screenshot.
  FeedbackUI::GetMostRecentScreenshots(
      temp_dir_.path(), &saved_screenshots_, 2);
  ASSERT_EQ(1U, saved_screenshots_.size());
  ASSERT_EQ(GetScreenshotUrl("20120416-152908"), saved_screenshots_[0]);
}

TEST_F(FeedbackUITest, GetMostRecentScreenshotsManyScreenShots) {
  // Create 2 screenshots.
  ASSERT_NO_FATAL_FAILURE(CreateScreenshotFile("20120416-152908"));
  ASSERT_NO_FATAL_FAILURE(CreateScreenshotFile("20120416-162908"));
  ASSERT_NO_FATAL_FAILURE(CreateScreenshotFile("20120413-152908"));
  // Expect getting most recent 2 screenshots.
  FeedbackUI::GetMostRecentScreenshots(
      temp_dir_.path(), &saved_screenshots_, 2);
  ASSERT_EQ(2U, saved_screenshots_.size());
  ASSERT_EQ(GetScreenshotUrl("20120416-162908"), saved_screenshots_[0]);
  ASSERT_EQ(GetScreenshotUrl("20120416-152908"), saved_screenshots_[1]);
}

}  // namespace
