// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"
#include "webkit/browser/fileapi/mock_file_system_options.h"

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

  base::MessageLoop message_loop;
  scoped_refptr<fileapi::ExternalMountPoints> mount_points =
      fileapi::ExternalMountPoints::CreateRefCounted();
  scoped_refptr<fileapi::FileSystemContext> context(
      new fileapi::FileSystemContext(
          base::MessageLoopProxy::current().get(),
          base::MessageLoopProxy::current().get(),
          mount_points.get(),
          NULL,  // special_storage_policy
          NULL,  // quota_manager_proxy,
          ScopedVector<fileapi::FileSystemBackend>(),
          temp_dir_.path(),  // partition_path
          fileapi::CreateAllowFileAccessOptions()));

  // Type:"external" + virtual_path:"drive/foo/bar" resolves to "drive/foo/bar".
  const std::string& drive_mount_name =
      GetDriveMountPointPath().BaseName().AsUTF8Unsafe();
  mount_points->RegisterFileSystem(
      drive_mount_name,
      fileapi::kFileSystemTypeDrive,
      GetDriveMountPointPath());
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe(drive_mount_name + "/foo/bar"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/" +
          drive_mount_name + "/foo/bar"))));

  // Virtual mount name should not affect the extracted path.
  mount_points->RevokeFileSystem(drive_mount_name);
  mount_points->RegisterFileSystem(
      "drive2",
      fileapi::kFileSystemTypeDrive,
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

TEST(FileSystemUtilTest, NormalizeFileName) {
  EXPECT_EQ("", NormalizeFileName(""));
  EXPECT_EQ("foo", NormalizeFileName("foo"));
  EXPECT_EQ("foo\xE2\x88\x95zzz", NormalizeFileName("foo/zzz"));
  EXPECT_EQ("\xE2\x88\x95\xE2\x88\x95\xE2\x88\x95", NormalizeFileName("///"));
  // Japanese hiragana "hi" + semi-voiced-mark is normalized to "pi".
  EXPECT_EQ("\xE3\x81\xB4", NormalizeFileName("\xE3\x81\xB2\xE3\x82\x9A"));
}

TEST(FileSystemUtilTest, GetCacheRootPath) {
  TestingProfile profile;
  base::FilePath profile_path = profile.GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(&profile));
}

TEST(FileSystemUtilTest, MigrateCacheFilesFromOldDirectories) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const base::FilePath persistent_directory =
      temp_dir.path().AppendASCII("persistent");
  const base::FilePath tmp_directory = temp_dir.path().AppendASCII("tmp");
  const base::FilePath files_directory =
      temp_dir.path().AppendASCII("files");

  // Prepare directories.
  ASSERT_TRUE(file_util::CreateDirectory(persistent_directory));
  ASSERT_TRUE(file_util::CreateDirectory(tmp_directory));
  ASSERT_TRUE(file_util::CreateDirectory(files_directory));

  // Put some files.
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      persistent_directory.AppendASCII("foo.abc"), "foo"));
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(
      tmp_directory.AppendASCII("bar.123"), "bar"));

  // Migrate.
  MigrateCacheFilesFromOldDirectories(temp_dir.path(),
                                      FILE_PATH_LITERAL("files"));

  EXPECT_FALSE(base::PathExists(persistent_directory));
  EXPECT_TRUE(base::PathExists(files_directory.AppendASCII("foo.abc")));
  EXPECT_TRUE(base::PathExists(files_directory.AppendASCII("bar.123")));
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

TEST(FileSystemUtilTest, GDocFile) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  GURL url("https://docs.google.com/document/d/"
           "1YsCnrMxxgp7LDdtlFDt-WdtEIth89vA9inrILtvK-Ug/edit");
  std::string resource_id("1YsCnrMxxgp7LDdtlFDt-WdtEIth89vA9inrILtvK-Ug");

  // Read and write gdoc.
  base::FilePath file = temp_dir.path().AppendASCII("test.gdoc");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gsheet.
  file = temp_dir.path().AppendASCII("test.gsheet");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gslides.
  file = temp_dir.path().AppendASCII("test.gslides");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gdraw.
  file = temp_dir.path().AppendASCII("test.gdraw");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gtable.
  file = temp_dir.path().AppendASCII("test.gtable");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write glink.
  file = temp_dir.path().AppendASCII("test.glink");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_TRUE(HasGDocFileExtension(file));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Non GDoc file.
  file = temp_dir.path().AppendASCII("test.txt");
  std::string data = "Hello world!";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(file, data));
  EXPECT_FALSE(HasGDocFileExtension(file));
  EXPECT_TRUE(ReadUrlFromGDocFile(file).is_empty());
  EXPECT_TRUE(ReadResourceIdFromGDocFile(file).empty());
}

TEST(FileSystemUtilTest, GetMd5Digest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.path().AppendASCII("test.txt");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(path, kTestData));

  EXPECT_EQ(base::MD5String(kTestData), GetMd5Digest(path));
}

}  // namespace util
}  // namespace drive
