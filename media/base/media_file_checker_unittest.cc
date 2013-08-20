// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "build/build_config.h"
#include "media/base/media_file_checker.h"
#include "media/base/test_data_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void RunMediaFileChecker(const std::string& filename, bool expectation) {
  base::PlatformFileError error;
  base::PlatformFile file = base::CreatePlatformFile(
      GetTestDataFilePath(filename),
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      &error);
  ASSERT_EQ(base::PLATFORM_FILE_OK, error);

  MediaFileChecker checker(file);
  const base::TimeDelta check_time = base::TimeDelta::FromMilliseconds(100);
  bool result = checker.Start(check_time);
  EXPECT_EQ(expectation, result);

  base::ClosePlatformFile(file);
}

TEST(MediaFileCheckerTest, InvalidFile) {
  RunMediaFileChecker("ten_byte_file", false);
}

TEST(MediaFileCheckerTest, Video) {
  RunMediaFileChecker("bear.ogv", true);
}

TEST(MediaFileCheckerTest, Audio) {
  RunMediaFileChecker("sfx.ogg", true);
}

#if defined(USE_PROPRIETARY_CODECS)
TEST(MediaFileCheckerTest, MP3) {
  RunMediaFileChecker("sfx.mp3", true);
}
#endif

}  // namespace media
