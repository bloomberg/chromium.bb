// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_util.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ExtensionFileUtil, MoveDirSafely) {
  // Create a test directory structure with some data in it.
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII("src");
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string data = "foobar";
  ASSERT_TRUE(file_util::WriteFile(src_path.AppendASCII("data"),
                                   data.c_str(), data.length()));

  // Move it to a path that doesn't exist yet.
  FilePath dest_path = temp.path().AppendASCII("dest").AppendASCII("dest");
  ASSERT_TRUE(extension_file_util::MoveDirSafely(src_path, dest_path));

  // The path should get created.
  ASSERT_TRUE(file_util::DirectoryExists(dest_path));

  // The data should match.
  std::string data_out;
  ASSERT_TRUE(file_util::ReadFileToString(dest_path.AppendASCII("data"),
                                          &data_out));
  ASSERT_EQ(data, data_out);

  // The src path should be gone.
  ASSERT_FALSE(file_util::PathExists(src_path));

  // Create some new test data.
  ASSERT_TRUE(file_util::CopyDirectory(dest_path, src_path,
                                       true));  // recursive
  data = "hotdog";
  ASSERT_TRUE(file_util::WriteFile(src_path.AppendASCII("data"),
                                   data.c_str(), data.length()));

  // Test again, overwriting the old path.
  ASSERT_TRUE(extension_file_util::MoveDirSafely(src_path, dest_path));
  ASSERT_TRUE(file_util::DirectoryExists(dest_path));

  data_out.clear();
  ASSERT_TRUE(file_util::ReadFileToString(dest_path.AppendASCII("data"),
                                          &data_out));
  ASSERT_EQ(data, data_out);
  ASSERT_FALSE(file_util::PathExists(src_path));
}

TEST(ExtensionFileUtil, SetCurrentVersion) {
  // Create an empty test directory.
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Set its version.
  std::string error;
  std::string version = "1.0";
  ASSERT_TRUE(extension_file_util::SetCurrentVersion(temp.path(), version,
                                                     &error));

  // Test the result.
  std::string version_out;
  ASSERT_TRUE(file_util::ReadFileToString(
      temp.path().AppendASCII(extension_file_util::kCurrentVersionFileName),
      &version_out));
  ASSERT_EQ(version, version_out);

  // Try again, overwriting the old value.
  version = "2.0";
  version_out.clear();
  ASSERT_TRUE(extension_file_util::SetCurrentVersion(temp.path(), version,
                                                     &error));
  ASSERT_TRUE(file_util::ReadFileToString(
      temp.path().AppendASCII(extension_file_util::kCurrentVersionFileName),
      &version_out));
  ASSERT_EQ(version, version_out);
}

TEST(ExtensionFileUtil, ReadCurrentVersion) {
  // Read the version from a valid extension.
  FilePath extension_path;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &extension_path));
  extension_path = extension_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj");

  std::string version;
  ASSERT_TRUE(extension_file_util::ReadCurrentVersion(extension_path,
                                                      &version));
  ASSERT_EQ("1.0.0.0", version);

  // Create an invalid extension and read its current version.
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  ASSERT_FALSE(extension_file_util::ReadCurrentVersion(temp.path(), &version));
}

TEST(ExtensionFileUtil, CompareToInstalledVersion) {
  // Compare to an existing extension.
  FilePath install_directory;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_directory));
  install_directory = install_directory.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");

  std::string id = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  std::string version = "1.0.0.0";
  std::string version_out;

  ASSERT_EQ(Extension::UPGRADE, extension_file_util::CompareToInstalledVersion(
    install_directory, id, "1.0.0.1", &version_out));
  ASSERT_EQ(version, version_out);

  version_out.clear();
  ASSERT_EQ(Extension::REINSTALL,
            extension_file_util::CompareToInstalledVersion(
                install_directory, id, "1.0.0.0", &version_out));
  ASSERT_EQ(version, version_out);

  version_out.clear();
  ASSERT_EQ(Extension::REINSTALL,
            extension_file_util::CompareToInstalledVersion(
                install_directory, id, "1.0.0", &version_out));
  ASSERT_EQ(version, version_out);

  version_out.clear();
  ASSERT_EQ(Extension::DOWNGRADE,
            extension_file_util::CompareToInstalledVersion(
                install_directory, id, "0.0.1.0", &version_out));
  ASSERT_EQ(version, version_out);

  // Compare to an extension that is missing its Current Version file.
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  FilePath src = install_directory.AppendASCII(id).AppendASCII(version);
  FilePath dest = temp.path().AppendASCII(id).AppendASCII(version);
  ASSERT_TRUE(file_util::CreateDirectory(dest.DirName()));
  ASSERT_TRUE(file_util::CopyDirectory(src, dest, true));

  version_out.clear();
  ASSERT_EQ(Extension::NEW_INSTALL,
            extension_file_util::CompareToInstalledVersion(
                temp.path(), id, "0.0.1.0", &version_out));
  ASSERT_EQ("", version_out);

  // Compare to a completely non-existent extension.
  version_out.clear();
  ASSERT_EQ(Extension::NEW_INSTALL,
            extension_file_util::CompareToInstalledVersion(
                temp.path(), "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx", "0.0.1.0",
                &version_out));
  ASSERT_EQ("", version_out);
}

// TODO(aa): More tests as motivation allows. Maybe steal some from
// ExtensionsService? Many of them could probably be tested here without the
// MessageLoop shenanigans.
