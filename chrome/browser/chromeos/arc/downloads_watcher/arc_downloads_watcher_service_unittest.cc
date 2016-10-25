// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/downloads_watcher/arc_downloads_watcher_service.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace arc {

TEST(ArcDownloadsWatcherServiceTest, HasAndroidSupportedMediaExtension) {
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.3g2"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.jpg"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.png"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.xmf"))));

  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.JPEG"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.cute.jpg"))));
  EXPECT_TRUE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/.kitten.jpg"))));

  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.txt"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten.jpg.exe"))));
  EXPECT_FALSE(HasAndroidSupportedMediaExtension(
      base::FilePath(FILE_PATH_LITERAL("/tmp/kitten"))));
}

}  // namespace arc
