// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/external_mount_points.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_url.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/mock_file_system_options.h"

namespace drive {
namespace util {

TEST(FileSystemUtilTest, FilePathToDriveURL) {
  base::FilePath path;

  // Path with alphabets and numbers.
  path = GetDriveMyDriveRootPath().AppendASCII("foo/bar012.txt");
  EXPECT_EQ(path, DriveURLToFilePath(FilePathToDriveURL(path)));

  // Path with symbols.
  path = GetDriveMyDriveRootPath().AppendASCII(
      " !\"#$%&'()*+,-.:;<=>?@[\\]^_`{|}~");
  EXPECT_EQ(path, DriveURLToFilePath(FilePathToDriveURL(path)));

  // Path with '%'.
  path = GetDriveMyDriveRootPath().AppendASCII("%19%20%21.txt");
  EXPECT_EQ(path, DriveURLToFilePath(FilePathToDriveURL(path)));

  // Path with multi byte characters.
  string16 utf16_string;
  utf16_string.push_back(0x307b);  // HIRAGANA_LETTER_HO
  utf16_string.push_back(0x3052);  // HIRAGANA_LETTER_GE
  path = GetDriveMyDriveRootPath().Append(
      base::FilePath::FromUTF8Unsafe(UTF16ToUTF8(utf16_string) + ".txt"));
  EXPECT_EQ(path, DriveURLToFilePath(FilePathToDriveURL(path)));
}

TEST(FileSystemUtilTest, IsUnderDriveMountPoint) {
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drivex/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("special/drivex/foo.txt")));

  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
}

TEST(FileSystemUtilTest, ExtractDrivePath) {
  EXPECT_EQ(base::FilePath(),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_EQ(base::FilePath(),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_EQ(base::FilePath(),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drivex/foo.txt")));

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive"),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/foo.txt"),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/subdir/foo.txt"),
            ExtractDrivePath(base::FilePath::FromUTF8Unsafe(
                "/special/drive/subdir/foo.txt")));
}

TEST(FileSystemUtilTest, ExtractDrivePathFromFileSystemUrl) {
  // Set up file system context for testing.
  base::ScopedTempDir temp_dir_;
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

  MessageLoop message_loop;
  scoped_refptr<fileapi::ExternalMountPoints> mount_points =
      fileapi::ExternalMountPoints::CreateRefCounted();
  scoped_refptr<fileapi::FileSystemContext> context(
      new fileapi::FileSystemContext(
          fileapi::FileSystemTaskRunners::CreateMockTaskRunners(),
          mount_points,
          NULL,  // special_storage_policy
          NULL,  // quota_manager_proxy,
          ScopedVector<fileapi::FileSystemMountPointProvider>(),
          temp_dir_.path(),  // partition_path
          fileapi::CreateAllowFileAccessOptions()));

  // Type:"external" + virtual_path:"drive/foo/bar" resolves to "drive/foo/bar".
  const std::string& drive_mount_name =
      GetDriveMountPointPath().BaseName().AsUTF8Unsafe();
  mount_points->RegisterRemoteFileSystem(
      drive_mount_name,
      fileapi::kFileSystemTypeDrive,
      NULL,  // RemoteFileSystemProxyInterface
      GetDriveMountPointPath());
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(drive_mount_name + "/foo/bar"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/" +
          drive_mount_name + "/foo/bar"))));

  // Virtual mount name should not affect the extracted path.
  mount_points->RevokeFileSystem(drive_mount_name);
  mount_points->RegisterRemoteFileSystem(
      "drive2",
      fileapi::kFileSystemTypeDrive,
      NULL,  // RemoteFileSystemProxyInterface
      GetDriveMountPointPath());
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(drive_mount_name + "/foo/bar"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/drive2/foo/bar"))));

  // Type:"external" + virtual_path:"Downloads/foo" is not a Drive path.
  mount_points->RegisterFileSystem(
      "Downloads",
      fileapi::kFileSystemTypeNativeLocal,
      temp_dir_.path());
  EXPECT_EQ(
      base::FilePath(),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/Downloads/foo"))));

  // Type:"isolated" + virtual_path:"isolated_id/name" mapped on a Drive path.
  std::string isolated_name;
  std::string isolated_id =
      fileapi::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeNativeForPlatformApp,
          GetDriveMountPointPath().AppendASCII("bar/buz"),
          &isolated_name);
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(drive_mount_name + "/bar/buz"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/isolated/" +
          isolated_id + "/" + isolated_name))));
}

TEST(FileSystemUtilTest, EscapeUnescapeCacheFileName) {
  const std::string kUnescapedFileName(
      "tmp:`~!@#$%^&*()-_=+[{|]}\\\\;\',<.>/?");
  const std::string kEscapedFileName(
      "tmp:`~!@#$%25^&*()-_=+[{|]}\\\\;\',<%2E>%2F?");
  EXPECT_EQ(kEscapedFileName, EscapeCacheFileName(kUnescapedFileName));
  EXPECT_EQ(kUnescapedFileName, UnescapeCacheFileName(kEscapedFileName));
}

TEST(FileSystemUtilTest, EscapeUtf8FileName) {
  EXPECT_EQ("", EscapeUtf8FileName(""));
  EXPECT_EQ("foo", EscapeUtf8FileName("foo"));
  EXPECT_EQ("foo\xE2\x88\x95zzz", EscapeUtf8FileName("foo/zzz"));
  EXPECT_EQ("\xE2\x88\x95\xE2\x88\x95\xE2\x88\x95", EscapeUtf8FileName("///"));
}

TEST(FileSystemUtilTest, ExtractResourceIdFromUrl) {
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file:2_file_resource_id")));
  // %3A should be unescaped.
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file%3A2_file_resource_id")));

  // The resource ID cannot be extracted, hence empty.
  EXPECT_EQ("", ExtractResourceIdFromUrl(GURL("https://www.example.com/")));
}

TEST(FileSystemUtilTest, GetCacheRootPath) {
  TestingProfile profile;
  base::FilePath profile_path = profile.GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(&profile));
}

TEST(FileSystemUtilTest, ParseCacheFilePath) {
  std::string resource_id, md5, extra_extension;
  ParseCacheFilePath(
      base::FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/persistent/pdf:a1b2.0123456789abcdef.mounted"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "mounted");

  ParseCacheFilePath(
      base::FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/tmp/pdf:a1b2.0123456789abcdef"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "0123456789abcdef");
  EXPECT_EQ(extra_extension, "");

  ParseCacheFilePath(
      base::FilePath::FromUTF8Unsafe(
          "/home/user/GCache/v1/pinned/pdf:a1b2"),
      &resource_id,
      &md5,
      &extra_extension);
  EXPECT_EQ(resource_id, "pdf:a1b2");
  EXPECT_EQ(md5, "");
  EXPECT_EQ(extra_extension, "");
}

TEST(FileSystemUtilTest, NeedsNamespaceMigration) {
  // Not Drive cases.
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/Downloads")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/Downloads/x")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drivex/foo.txt")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("special/drivex/foo.txt")));

  // Before migration.
  EXPECT_TRUE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_TRUE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_TRUE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));

  // Already migrated.
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/root")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/root/dir1")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/root/root")));
  EXPECT_FALSE(NeedsNamespaceMigration(
      base::FilePath::FromUTF8Unsafe("/special/drive/root/root/dir1")));
}

TEST(FileSystemUtilTest, ConvertToMyDriveNamespace) {
  // Migration cases.
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive/root"),
            drive::util::ConvertToMyDriveNamespace(
                base::FilePath::FromUTF8Unsafe("/special/drive")));

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive/root/dir1"),
            drive::util::ConvertToMyDriveNamespace(
                base::FilePath::FromUTF8Unsafe("/special/drive/dir1")));
}

TEST(FileSystemUtilTest, IsSpecialResourceId) {
  EXPECT_FALSE(util::IsSpecialResourceId("abc"));
  EXPECT_FALSE(util::IsSpecialResourceId("file:123"));
  EXPECT_FALSE(util::IsSpecialResourceId("folder:root"));
  EXPECT_FALSE(util::IsSpecialResourceId("folder:xyz"));

  EXPECT_TRUE(util::IsSpecialResourceId("<drive>"));
  EXPECT_TRUE(util::IsSpecialResourceId("<other>"));
}

}  // namespace util
}  // namespace drive
