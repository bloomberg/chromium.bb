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
  EXPECT_EQ(0, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.exe."))));
  EXPECT_EQ(0, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.exe "))));
  EXPECT_EQ(0, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.exe.. . . .   "))));
  EXPECT_EQ(9, GetSBClientDownloadExtensionValueForUMA(
                   base::FilePath(FILE_PATH_LITERAL("foo.dll"))));
  EXPECT_EQ(29, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.jse"))));
  EXPECT_EQ(18, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.111"))));
  EXPECT_EQ(18, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.zzz"))));
  EXPECT_EQ(152, GetSBClientDownloadExtensionValueForUMA(
                    base::FilePath(FILE_PATH_LITERAL("foo.tbz2"))));
  EXPECT_TRUE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.zip"))));
  EXPECT_TRUE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.zip."))));
  EXPECT_TRUE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.zip "))));
  EXPECT_TRUE(
      IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.zip   ...."))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.zip"))));
  EXPECT_TRUE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.rar"))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.rar"))));
  EXPECT_FALSE(IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.ps1"))));
  EXPECT_FALSE(
      IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("foo.unknownextension"))));
  EXPECT_FALSE(
      IsArchiveFile(base::FilePath(FILE_PATH_LITERAL("no_extension"))));
  EXPECT_TRUE(IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL(".exe"))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.msi"))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.msi."))));
  EXPECT_TRUE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.msi "))));
  EXPECT_TRUE(IsSupportedBinaryFile(
      base::FilePath(FILE_PATH_LITERAL("foo.msi  .. .."))));
  EXPECT_FALSE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("foo.txt"))));
  EXPECT_FALSE(IsSupportedBinaryFile(
      base::FilePath(FILE_PATH_LITERAL("foo.unknownextension"))));
  EXPECT_FALSE(
      IsSupportedBinaryFile(base::FilePath(FILE_PATH_LITERAL("no_extension"))));

  EXPECT_EQ(ClientDownloadRequest::WIN_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.exe"))));
  EXPECT_EQ(ClientDownloadRequest::CHROME_EXTENSION,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.crx"))));
  EXPECT_EQ(ClientDownloadRequest::ZIPPED_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.zip"))));
  EXPECT_EQ(ClientDownloadRequest::ARCHIVE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.rar"))));
  EXPECT_EQ(ClientDownloadRequest::MAC_EXECUTABLE,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.pkg"))));
  EXPECT_EQ(ClientDownloadRequest::ANDROID_APK,
            GetDownloadType(base::FilePath(FILE_PATH_LITERAL("foo.apk"))));
}

} // namespace download_protection_util
} // namespace safe_browsing


