// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_file_util.h"

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

TEST(ExtensionFileUtil, InstallUninstallGarbageCollect) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // Create a source extension.
  std::string extension_id("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
  std::string version("1.0");
  FilePath src = temp.path().AppendASCII(extension_id);
  ASSERT_TRUE(file_util::CreateDirectory(src));

  // Create a extensions tree.
  FilePath all_extensions = temp.path().AppendASCII("extensions");
  ASSERT_TRUE(file_util::CreateDirectory(all_extensions));

  // Install in empty directory. Should create parent directories as needed.
  FilePath version_1 = extension_file_util::InstallExtension(src,
                                                             extension_id,
                                                             version,
                                                             all_extensions);
  ASSERT_EQ(version_1.value(),
            all_extensions.AppendASCII(extension_id).AppendASCII("1.0_0")
                .value());
  ASSERT_TRUE(file_util::DirectoryExists(version_1));

  // Should have moved the source.
  ASSERT_FALSE(file_util::DirectoryExists(src));

  // Install again. Should create a new one with different name.
  ASSERT_TRUE(file_util::CreateDirectory(src));
  FilePath version_2 = extension_file_util::InstallExtension(src,
                                                             extension_id,
                                                             version,
                                                             all_extensions);
  ASSERT_EQ(version_2.value(),
            all_extensions.AppendASCII(extension_id).AppendASCII("1.0_1")
                .value());
  ASSERT_TRUE(file_util::DirectoryExists(version_2));

  // Collect garbage. Should remove first one.
  std::map<std::string, FilePath> extension_paths;
  extension_paths[extension_id] =
      FilePath().AppendASCII(extension_id).Append(version_2.BaseName());
  extension_file_util::GarbageCollectExtensions(all_extensions,
                                                extension_paths);
  ASSERT_FALSE(file_util::DirectoryExists(version_1));
  ASSERT_TRUE(file_util::DirectoryExists(version_2));

  // Uninstall. Should remove entire extension subtree.
  extension_file_util::UninstallExtension(all_extensions, extension_id);
  ASSERT_FALSE(file_util::DirectoryExists(version_2.DirName()));
  ASSERT_TRUE(file_util::DirectoryExists(all_extensions));
}

TEST(ExtensionFileUtil, LoadExtensionWithValidLocales) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::string error;
  scoped_refptr<Extension> extension(extension_file_util::LoadExtension(
      install_dir, Extension::LOAD, false, true, &error));
  ASSERT_TRUE(extension != NULL);
  EXPECT_EQ("The first extension that I made.", extension->description());
}

TEST(ExtensionFileUtil, LoadExtensionWithoutLocalesFolder) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
      .AppendASCII("1.0");

  std::string error;
  scoped_refptr<Extension> extension(extension_file_util::LoadExtension(
      install_dir, Extension::LOAD, false, true, &error));
  ASSERT_FALSE(extension == NULL);
  EXPECT_TRUE(error.empty());
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesNoUnderscores) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII("some_dir");
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string data = "{ \"name\": { \"message\": \"foobar\" } }";
  ASSERT_TRUE(file_util::WriteFile(src_path.AppendASCII("some_file.txt"),
                                   data.c_str(), data.length()));
  std::string error;
  EXPECT_TRUE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                            &error));
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesOnlyReserved) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_TRUE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                            &error));
}

TEST(ExtensionFileUtil, CheckIllegalFilenamesReservedAndIllegal) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  src_path = temp.path().AppendASCII("_some_dir");
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_FALSE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                             &error));
}

TEST(ExtensionFileUtil, LoadExtensionGivesHelpfullErrorOnMissingManifest) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("bad")
      .AppendASCII("Extensions")
      .AppendASCII("dddddddddddddddddddddddddddddddd")
      .AppendASCII("1.0");

  std::string error;
  scoped_refptr<Extension> extension(extension_file_util::LoadExtension(
      install_dir, Extension::LOAD, false, true, &error));
  ASSERT_TRUE(extension == NULL);
  ASSERT_FALSE(error.empty());
  ASSERT_STREQ("Manifest file is missing or unreadable.", error.c_str());
}

TEST(ExtensionFileUtil, LoadExtensionGivesHelpfullErrorOnBadManifest) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("bad")
      .AppendASCII("Extensions")
      .AppendASCII("eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee")
      .AppendASCII("1.0");

  std::string error;
  scoped_refptr<Extension> extension(extension_file_util::LoadExtension(
      install_dir, Extension::LOAD, false, true, &error));
  ASSERT_TRUE(extension == NULL);
  ASSERT_FALSE(error.empty());
  ASSERT_STREQ("Manifest is not valid JSON.  "
               "Line: 2, column: 16, Syntax error.", error.c_str());
}

TEST(ExtensionFileUtil, FailLoadingNonUTF8Scripts) {
  FilePath install_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &install_dir));
  install_dir = install_dir.AppendASCII("extensions")
      .AppendASCII("bad")
      .AppendASCII("bad_encoding");

  std::string error;
  scoped_refptr<Extension> extension(extension_file_util::LoadExtension(
      install_dir, Extension::LOAD, false, true, &error));
  ASSERT_TRUE(extension == NULL);
  ASSERT_STREQ("Could not load file 'bad_encoding.js' for content script. "
               "It isn't UTF-8 encoded.", error.c_str());
}

#define URL_PREFIX "chrome-extension://extension-id/"

TEST(ExtensionFileUtil, ExtensionURLToRelativeFilePath) {
  struct TestCase {
    const char* url;
    const char* expected_relative_path;
  } test_cases[] = {
    { URL_PREFIX "simple.html",
      "simple.html" },
    { URL_PREFIX "directory/to/file.html",
      "directory/to/file.html" },
    { URL_PREFIX "escape%20spaces.html",
      "escape spaces.html" },
    { URL_PREFIX "%C3%9Cber.html",
      "\xC3\x9C" "ber.html" },
#if defined(OS_WIN)
    { URL_PREFIX "C%3A/simple.html",
      "" },
#endif
    { URL_PREFIX "////simple.html",
      "simple.html" },
    { URL_PREFIX "/simple.html",
      "simple.html" },
    { URL_PREFIX "\\simple.html",
      "simple.html" },
    { URL_PREFIX "\\\\foo\\simple.html",
      "foo/simple.html" },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    GURL url(test_cases[i].url);
#if defined(OS_POSIX)
    FilePath expected_path(test_cases[i].expected_relative_path);
#elif defined(OS_WIN)
    FilePath expected_path(UTF8ToWide(test_cases[i].expected_relative_path));
#endif

    FilePath actual_path =
        extension_file_util::ExtensionURLToRelativeFilePath(url);
    EXPECT_FALSE(actual_path.IsAbsolute()) <<
      " For the path " << actual_path.value();
    EXPECT_EQ(expected_path.value(), actual_path.value()) <<
      " For the path " << url;
  }
}

// TODO(aa): More tests as motivation allows. Maybe steal some from
// ExtensionService? Many of them could probably be tested here without the
// MessageLoop shenanigans.
