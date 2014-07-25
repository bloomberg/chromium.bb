// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/file_system_util.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_file_system_options.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/browser/fileapi/external_mount_points.h"
#include "webkit/browser/fileapi/file_system_backend.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/isolated_context.h"

namespace drive {
namespace util {

namespace {

// Sets up ProfileManager for testing and marks the current thread as UI by
// TestBrowserThreadBundle. We need the thread since Profile objects must be
// touched from UI and hence has CHECK/DCHECKs for it.
class ProfileRelatedFileSystemUtilTest : public testing::Test {
 protected:
  ProfileRelatedFileSystemUtilTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(testing_profile_manager_.SetUp());
  }

  Profile* CreateProfileWithName(const std::string& name) {
    return testing_profile_manager_.CreateTestingProfile(
        chrome::kProfileDirPrefix + name);
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfileManager testing_profile_manager_;
};

}  // namespace

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
  base::string16 utf16_string;
  utf16_string.push_back(0x307b);  // HIRAGANA_LETTER_HO
  utf16_string.push_back(0x3052);  // HIRAGANA_LETTER_GE
  path = GetDriveMyDriveRootPath().Append(
      base::FilePath::FromUTF8Unsafe(base::UTF16ToUTF8(utf16_string) + ".txt"));
  EXPECT_EQ(path, DriveURLToFilePath(FilePathToDriveURL(path)));
}

TEST_F(ProfileRelatedFileSystemUtilTest, GetDriveMountPointPath) {
  Profile* profile = CreateProfileWithName("hash1");
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("/special/drive-hash1"),
            GetDriveMountPointPath(profile));
}

TEST_F(ProfileRelatedFileSystemUtilTest, ExtractProfileFromPath) {
  Profile* profile1 = CreateProfileWithName("hash1");
  Profile* profile2 = CreateProfileWithName("hash2");
  EXPECT_EQ(profile1, ExtractProfileFromPath(
      base::FilePath::FromUTF8Unsafe("/special/drive-hash1")));
  EXPECT_EQ(profile2, ExtractProfileFromPath(
      base::FilePath::FromUTF8Unsafe("/special/drive-hash2/root/xxx")));
  EXPECT_EQ(NULL, ExtractProfileFromPath(
      base::FilePath::FromUTF8Unsafe("/special/non-drive-path")));
}

TEST(FileSystemUtilTest, IsUnderDriveMountPoint) {
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/foo.txt")));
  EXPECT_FALSE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("special/drive/foo.txt")));

  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive/subdir/foo.txt")));
  EXPECT_TRUE(IsUnderDriveMountPoint(
      base::FilePath::FromUTF8Unsafe("/special/drive-xxx/foo.txt")));
}

TEST(FileSystemUtilTest, ExtractDrivePath) {
  EXPECT_EQ(base::FilePath(),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/wherever/foo.txt")));
  EXPECT_EQ(base::FilePath(),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/foo.txt")));

  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive"),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drive")));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/foo.txt"),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drive/foo.txt")));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/subdir/foo.txt"),
            ExtractDrivePath(base::FilePath::FromUTF8Unsafe(
                "/special/drive/subdir/foo.txt")));
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe("drive/foo.txt"),
            ExtractDrivePath(
                base::FilePath::FromUTF8Unsafe("/special/drive-xxx/foo.txt")));
}

TEST(FileSystemUtilTest, ExtractDrivePathFromFileSystemUrl) {
  TestingProfile profile;

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
          std::vector<fileapi::URLRequestAutoMountHandler>(),
          temp_dir_.path(),  // partition_path
          content::CreateAllowFileAccessOptions()));

  // Type:"external" + virtual_path:"drive/foo/bar" resolves to "drive/foo/bar".
  const std::string& drive_mount_name =
      GetDriveMountPointPath(&profile).BaseName().AsUTF8Unsafe();
  mount_points->RegisterFileSystem(
      drive_mount_name,
      fileapi::kFileSystemTypeDrive,
      fileapi::FileSystemMountOption(),
      GetDriveMountPointPath(&profile));
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe("drive/foo/bar"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/" +
          drive_mount_name + "/foo/bar"))));

  // Virtual mount name should not affect the extracted path.
  mount_points->RevokeFileSystem(drive_mount_name);
  mount_points->RegisterFileSystem(
      "drive2",
      fileapi::kFileSystemTypeDrive,
      fileapi::FileSystemMountOption(),
      GetDriveMountPointPath(&profile));
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe("drive/foo/bar"),
      ExtractDrivePathFromFileSystemUrl(context->CrackURL(GURL(
          "filesystem:chrome-extension://dummy-id/external/drive2/foo/bar"))));

  // Type:"external" + virtual_path:"Downloads/foo" is not a Drive path.
  mount_points->RegisterFileSystem(
      "Downloads",
      fileapi::kFileSystemTypeNativeLocal,
      fileapi::FileSystemMountOption(),
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
          std::string(),
          GetDriveMountPointPath(&profile).AppendASCII("bar/buz"),
          &isolated_name);
  EXPECT_EQ(
      base::FilePath::FromUTF8Unsafe("drive/bar/buz"),
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
  // Slash
  EXPECT_EQ("foo_zzz", NormalizeFileName("foo/zzz"));
  EXPECT_EQ("___", NormalizeFileName("///"));
  // Japanese hiragana "hi" + semi-voiced-mark is normalized to "pi".
  EXPECT_EQ("\xE3\x81\xB4", NormalizeFileName("\xE3\x81\xB2\xE3\x82\x9A"));
  // Dot
  EXPECT_EQ("_", NormalizeFileName("."));
  EXPECT_EQ("_", NormalizeFileName(".."));
  EXPECT_EQ("_", NormalizeFileName("..."));
  EXPECT_EQ(".bashrc", NormalizeFileName(".bashrc"));
  EXPECT_EQ("._", NormalizeFileName("./"));
}

TEST(FileSystemUtilTest, GetCacheRootPath) {
  TestingProfile profile;
  base::FilePath profile_path = profile.GetPath();
  EXPECT_EQ(profile_path.AppendASCII("GCache/v1"),
            util::GetCacheRootPath(&profile));
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
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gsheet.
  file = temp_dir.path().AppendASCII("test.gsheet");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gslides.
  file = temp_dir.path().AppendASCII("test.gslides");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gdraw.
  file = temp_dir.path().AppendASCII("test.gdraw");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Read and write gtable.
  file = temp_dir.path().AppendASCII("test.gtable");
  EXPECT_TRUE(CreateGDocFile(file, url, resource_id));
  EXPECT_EQ(url, ReadUrlFromGDocFile(file));
  EXPECT_EQ(resource_id, ReadResourceIdFromGDocFile(file));

  // Non GDoc file.
  file = temp_dir.path().AppendASCII("test.txt");
  std::string data = "Hello world!";
  EXPECT_TRUE(google_apis::test_util::WriteStringToFile(file, data));
  EXPECT_TRUE(ReadUrlFromGDocFile(file).is_empty());
  EXPECT_TRUE(ReadResourceIdFromGDocFile(file).empty());
}

}  // namespace util
}  // namespace drive
