// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/download_protection_util.h"

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace download_protection_util {

TEST(DownloadProtectionUtilTest, KnownValues) {
  EXPECT_EQ(0, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.exe"))));
  EXPECT_EQ(9, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.dll"))));
  EXPECT_EQ(29, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.jse"))));
  EXPECT_EQ(18, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.111"))));
  EXPECT_EQ(18, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.zzz"))));
  EXPECT_TRUE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.zip"))));
  EXPECT_FALSE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.ps1"))));
  EXPECT_FALSE(
      IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.unknownextension"))));
  EXPECT_FALSE(
      IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("no_extension"))));
  EXPECT_TRUE(IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL(".exe"))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.msi"))));
  EXPECT_FALSE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.txt"))));
  EXPECT_FALSE(IsSupportedBinaryFile(
      base::FilePath(FILE_PATH_LITERAL("foo.unknownextension"))));
  EXPECT_FALSE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("no_extension"))));
}

} // namespace download_protection_util
} // namespace safe_browsing


