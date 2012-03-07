// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include "base/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gdata {
namespace util {

TEST(GDataUtilTest, GetGDataMountPointPath) {
  EXPECT_EQ(FilePath::FromUTF8Unsafe("/special/gdata"),
            GetGDataMountPointPath());
}

TEST(GDataUtilTest, GetGDataMountPointPathAsString) {
  EXPECT_EQ("/special/gdata", GetGDataMountPointPathAsString());
}

TEST(GDataUtilTest, IsUnderGDataMountPoint) {
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/gdatax/foo.txt")));
  EXPECT_FALSE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("special/gdatax/foo.txt")));

  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/gdata")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/gdata/foo.txt")));
  EXPECT_TRUE(IsUnderGDataMountPoint(
      FilePath::FromUTF8Unsafe("/special/gdata/subdir/foo.txt")));
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

  EXPECT_EQ(FilePath::FromUTF8Unsafe("gdata"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/gdata")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("gdata/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/gdata/foo.txt")));
  EXPECT_EQ(FilePath::FromUTF8Unsafe("gdata/subdir/foo.txt"),
            ExtractGDataPath(
                FilePath::FromUTF8Unsafe("/special/gdata/subdir/foo.txt")));
}

}  // namespace util
}  // namespace gdata
