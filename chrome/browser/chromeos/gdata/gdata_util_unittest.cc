// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include "base/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestResponse[] =
"<?xml version='1.0' encoding='UTF-8'?> "
"<entry "
"    xmlns='http://www.w3.org/2005/Atom' "
"    xmlns:docs='http://schemas.google.com/docs/2007' "
"    xmlns:batch='http://schemas.google.com/gdata/batch' "
"    xmlns:gd='http://schemas.google.com/g/2005' "
"    gd:etag='&quot;E1dWUEldRyt7ImBr&quot;'> "
"  <id>https://docs.google.com/feeds/id/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ</id> "
"  <published>2012-03-12T23:41:38.112Z</published> "
"  <updated>2012-03-12T23:41:38.112Z</updated> "
"  <app:edited "
"     xmlns:app='http://www.w3.org/2007/app'>2012-03-12T23:41:38.112Z</app:edited> "
"  <category scheme='http://schemas.google.com/g/2005#kind' "
"     term='http://schemas.google.com/docs/2007#file' label='image/jpeg'/> "
"  <category scheme='http://schemas.google.com/g/2005/labels' "
"     term='http://schemas.google.com/g/2005/labels#modified-by-me' "
"     label='modified-by-me'/> "
"  <title>reims.jpg</title> "
"  <content type='image/jpeg' "
"     src='https://doc-0k-4o-docs.googleusercontent.com/docs/securesc/976uu9oe76h0pjnve35kjeggik6ajkhb/fq7kfppdimeprq02c5ppnn23ks58r9l8/1331589600000/15686861778668347995/15686861778668347995/0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ?h=04772779868826822459&amp;e=download&amp;gd=true'/> "
"  <link rel='alternate' type='text/html' "
"     href='https://docs.google.com/a/chromium.org/file/d/0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ/edit'/> "
"  <link rel='http://schemas.google.com/docs/2007#icon' type='image/png' "
"     href='https://ssl.gstatic.com/docs/doclist/images/icon_9_image_list.png'/> "
"  <link rel='http://schemas.google.com/g/2005#resumable-edit-media' "
"     type='application/atom+xml' "
"     href='https://docs.google.com/feeds/upload/create-session/default/private/full/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ'/> "
"  <link rel='http://schemas.google.com/docs/2007/thumbnail' type='image/jpeg' "
"     href='https://lh4.googleusercontent.com/E8hor0qL-M7uO5eq-wD0wqT6stSevFPRxatAzk3xMo3ljNhZqWNI0mF4_VIYWObSvc3ZniCWUJy7BsNc=s220'/> "
"  <link rel='self' type='application/atom+xml' "
"     href='https://docs.google.com/feeds/default/private/full/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ'/> "
"  <link rel='edit' type='application/atom+xml' "
"     href='https://docs.google.com/feeds/default/private/full/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ'/> "
"  <link rel='edit-media' type='image/jpeg' "
"     href='https://docs.google.com/feeds/default/media/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ'/> "
"  <author><name>achuith</name><email>achuith@chromium.org</email></author> "
"  <gd:resourceId>file:0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ</gd:resourceId> "
"  <docs:modifiedByMeDate>2012-03-12T23:41:37.552Z</docs:modifiedByMeDate> "
"  <gd:lastModifiedBy><name>achuith</name><email>achuith@chromium.org</email></gd:lastModifiedBy> "
"  <gd:quotaBytesUsed>483021</gd:quotaBytesUsed> "
"  <docs:writersCanInvite value='true'/> "
"  <docs:md5Checksum>b52eb08b04e20674ce7a780165623048</docs:md5Checksum> "
"  <docs:filename>reims.jpg</docs:filename> "
"  <docs:suggestedFilename>reims.jpg</docs:suggestedFilename> "
"  <docs:size>483021</docs:size> "
"  <gd:feedLink rel='http://schemas.google.com/acl/2007#accessControlList' "
"     href='https://docs.google.com/feeds/default/private/full/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ/acl'/> "
"  <gd:feedLink rel='http://schemas.google.com/docs/2007/revisions' "
"     href='https://docs.google.com/feeds/default/private/full/file%3A0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ/revisions'/> "
"</entry> ";

}  // namespace

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

TEST(GDataUtilTest, ParseCreatedResponseContentTest) {
  std::string resource_id;
  std::string md5_checksum;

  ParseCreatedResponseContent("", &resource_id, &md5_checksum);
  EXPECT_TRUE(resource_id.empty());
  EXPECT_TRUE(md5_checksum.empty());

  ParseCreatedResponseContent(kTestResponse, &resource_id, &md5_checksum);
  EXPECT_EQ("file:0BzbEd5HsutYuVkQwQS1QMWtUS0tLU0Y1RmJfbFR5UQ", resource_id);
  EXPECT_EQ("b52eb08b04e20674ce7a780165623048", md5_checksum);
}

}  // namespace util
}  // namespace gdata
