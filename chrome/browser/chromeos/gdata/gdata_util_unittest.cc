// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include "base/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace util {

TEST(GDataUtilTest, IsUnderGDataMountPoint) {
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drivex/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("special/drivex/foo.txt")));

  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
}

TEST(GDataUtilTest, ExtractGDataPath) {
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_EQ(FilePath(),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/gdatax/foo.txt")));

  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("drive/subdir/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
}

TEST(GDataUtilTest, EscapeUnescapeCacheFileName) {
  const std::string kUnescapedFileName(
      "tmp:`~!@#$%^&*()-_=+[{|]}\\\\;\',<.>/?");
  const std::string kEscapedFileName(
      "tmp:`~!@#$%25^&*()-_=+[{|]}\\\\;\',<%2E>%2F?");
  EXPECT_EQ(kEscapedFileName, EscapeCacheFileName(kUnescapedFileName));
  EXPECT_EQ(kUnescapedFileName, UnescapeCacheFileName(kEscapedFileName));
}

TEST(GDataUtilTest, ParseCacheFilePath) {
  std::string resource_id, md5, extra_extension;
  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/persistent/pdf:a1b2.0123456789abcdef.mounted"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "mounted");

  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/tmp/pdf:a1b2.0123456789abcdef"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "");

  ParseCacheFilePath(
      FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/pinned/pdf:a1b2"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "");
  EXPECT_EQ(extra_extension, "");
}

}  // namespace util
}  // namespace gdata
