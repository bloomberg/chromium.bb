// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_file_util.h"

#include "base/file_util.h"
#include "base/scoped_temp_dir.h"
#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

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

TEST(ExtensionFileUtil, CompareToInstalledVersion) {
  // Compare to an existing extension.
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions");

  const std::string kId = "behllobkkfkfnphdnhnkndlbkcpglgmj";
  const std::string kCurrentVersion = "1.0.0.0";

  FilePath version_dir;

  ASSERT_EQ(Extension::UPGRADE,
            extension_file_util::CompareToInstalledVersion(
                install_dir, kId, kCurrentVersion, "1.0.0.1", &version_dir));

  ASSERT_EQ(Extension::REINSTALL,
            extension_file_util::CompareToInstalledVersion(
                install_dir, kId, kCurrentVersion, "1.0.0.0", &version_dir));

  ASSERT_EQ(Extension::REINSTALL,
            extension_file_util::CompareToInstalledVersion(
                install_dir, kId, kCurrentVersion, "1.0.0", &version_dir));

  ASSERT_EQ(Extension::DOWNGRADE,
            extension_file_util::CompareToInstalledVersion(
                install_dir, kId, kCurrentVersion, "0.0.1.0", &version_dir));

  // Compare to an extension that is missing its manifest file.
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  FilePath src = install_dir.AppendASCII(kId).AppendASCII(kCurrentVersion);
  FilePath dest = temp.path().AppendASCII(kId).AppendASCII(kCurrentVersion);
  ASSERT_TRUE(file_util::CreateDirectory(dest.DirName()));
  ASSERT_TRUE(file_util::CopyDirectory(src, dest, true));
  ASSERT_TRUE(file_util::Delete(dest.AppendASCII("manifest.json"), false));

  ASSERT_EQ(Extension::NEW_INSTALL,
            extension_file_util::CompareToInstalledVersion(
                temp.path(), kId, kCurrentVersion, "1.0.0", &version_dir));

  // Compare to a non-existent extension.
  const std::string kMissingVersion = "";
  const std::string kBadId = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
  ASSERT_EQ(Extension::NEW_INSTALL,
            extension_file_util::CompareToInstalledVersion(
                temp.path(), kBadId, kMissingVersion, "1.0.0", &version_dir));
}

// Creates minimal manifest, with or without default_locale section.
bool CreateMinimalManifest(const std::string& locale,
                           const FilePath& manifest_path) {
  DictionaryValue manifest;

  manifest.SetString(keys::kVersion, "1.0.0.0");
  manifest.SetString(keys::kName, "my extension");
  if (!locale.empty()) {
    manifest.SetString(keys::kDefaultLocale, locale);
  }

  JSONFileValueSerializer serializer(manifest_path);
  return serializer.Serialize(manifest);
}

TEST(ExtensionFileUtil, LoadExtensionWithValidLocales) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  ASSERT_TRUE(CreateMinimalManifest(
      "en_US", temp.path().AppendASCII(Extension::kManifestFilename)));

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  FilePath locale_1 = src_path.AppendASCII("sr");
  ASSERT_TRUE(file_util::CreateDirectory(locale_1));

  std::string data = "foobar";
  ASSERT_TRUE(
      file_util::WriteFile(locale_1.AppendASCII(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  FilePath locale_2 = src_path.AppendASCII("en_US");
  ASSERT_TRUE(file_util::CreateDirectory(locale_2));

  ASSERT_TRUE(
      file_util::WriteFile(locale_2.AppendASCII(Extension::kMessagesFilename),
                           data.c_str(), data.length()));

  std::string error;
  scoped_ptr<Extension> extension(
      extension_file_util::LoadExtension(temp.path(), false, &error));
  ASSERT_FALSE(extension == NULL);
  EXPECT_EQ(static_cast<unsigned int>(2),
            extension->supported_locales().size());
  EXPECT_EQ("en-US", extension->default_locale());
}

TEST(ExtensionFileUtil, LoadExtensionWithoutLocalesFolder) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());
  ASSERT_TRUE(CreateMinimalManifest(
      "", temp.path().AppendASCII(Extension::kManifestFilename)));

  std::string error;
  scoped_ptr<Extension> extension(
      extension_file_util::LoadExtension(temp.path(), false, &error));
  ASSERT_FALSE(extension == NULL);
  EXPECT_TRUE(extension->supported_locales().empty());
  EXPECT_TRUE(extension->default_locale().empty());
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesNoUnderscores) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII("some_dir");
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string data = "foobar";
  ASSERT_TRUE(file_util::WriteFile(src_path.AppendASCII("some_file.txt"),
                                   data.c_str(), data.length()));
  std::string error;
  EXPECT_TRUE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                            &error));
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesOnlyReserved) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_TRUE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                            &error));
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesReservedAndIllegal) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  src_path = temp.path().AppendASCII("_some_dir");
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                             &error));
}

// TODO(aa): More tests as motivation allows. Maybe steal some from
// ExtensionsService? Many of them could probably be tested here without the
// MessageLoop shenanigans.
