// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_file_util.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace keys = extension_manifest_keys;

#if defined(OS_WIN)
// http://crbug.com/106381
#define InstallUninstallGarbageCollect DISABLED_InstallUninstallGarbageCollect
#endif
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
      install_dir, Extension::LOAD, Extension::STRICT_ERROR_CHECKS, &error));
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
      install_dir, Extension::LOAD, Extension::STRICT_ERROR_CHECKS, &error));
  ASSERT_FALSE(extension == NULL);
  EXPECT_TRUE(error.empty());
}

#if defined(OS_WIN)
// http://crbug.com/106381
#define CheckIllegalFilenamesNoUnderscores \
    DISABLED_CheckIllegalFilenamesNoUnderscores
#endif
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

#if defined(OS_WIN)
// http://crbug.com/106381
#define CheckIllegalFilenamesOnlyReserved \
    DISABLED_CheckIllegalFilenamesOnlyReserved
#endif
TEST(ExtensionFileUtil, CheckIllegalFilenamesOnlyReserved) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().Append(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  std::string error;
  EXPECT_TRUE(extension_file_util::CheckForIllegalFilenames(temp.path(),
                                                            &error));
}

#if defined(OS_WIN)
// http://crbug.com/106381
#define CheckIllegalFilenamesReservedAndIllegal \
    DISABLED_CheckIllegalFilenamesReservedAndIllegal
#endif
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
      install_dir, Extension::LOAD, Extension::STRICT_ERROR_CHECKS, &error));
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
      install_dir, Extension::LOAD, Extension::STRICT_ERROR_CHECKS, &error));
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
      install_dir, Extension::LOAD, Extension::STRICT_ERROR_CHECKS, &error));
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

static scoped_refptr<Extension> LoadExtensionManifest(
    DictionaryValue* manifest,
    const FilePath& manifest_dir,
    Extension::Location location,
    int extra_flags,
    std::string* error) {
  scoped_refptr<Extension> extension = Extension::Create(
      manifest_dir, location, *manifest, extra_flags, error);
  return extension;
}

static scoped_refptr<Extension> LoadExtensionManifest(
    const std::string& manifest_value,
    const FilePath& manifest_dir,
    Extension::Location location,
    int extra_flags,
    std::string* error) {
  JSONStringValueSerializer serializer(manifest_value);
  scoped_ptr<Value> result(serializer.Deserialize(NULL, error));
  if (!result.get())
    return NULL;
  CHECK_EQ(Value::TYPE_DICTIONARY, result->GetType());
  return LoadExtensionManifest(static_cast<DictionaryValue*>(result.get()),
                               manifest_dir,
                               location,
                               extra_flags,
                               error);
}

#if defined(OS_WIN)
// http://crbug.com/108279
#define ValidateThemeUTF8 DISABLED_ValidateThemeUTF8
#endif
TEST(ExtensionFileUtil, ValidateThemeUTF8) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  // "aeo" with accents. Use http://0xcc.net/jsescape/ to decode them.
  std::string non_ascii_file = "\xC3\xA0\xC3\xA8\xC3\xB2.png";
  FilePath non_ascii_path = temp.path().Append(FilePath::FromUTF8Unsafe(
      non_ascii_file));
  file_util::WriteFile(non_ascii_path, "", 0);

  std::string kManifest =
      base::StringPrintf(
          "{ \"name\": \"Test\", \"version\": \"1.0\", "
          "  \"theme\": { \"images\": { \"theme_frame\": \"%s\" } }"
          "}", non_ascii_file.c_str());
  std::string error;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      kManifest, temp.path(), Extension::LOAD, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_TRUE(extension_file_util::ValidateExtension(extension, &error)) <<
      error;
}

#if defined(OS_WIN)
// This test hangs on Windows sometimes. http://crbug.com/110279
#define MAYBE_BackgroundScriptsMustExist DISABLED_BackgroundScriptsMustExist
#else
#define MAYBE_BackgroundScriptsMustExist BackgroundScriptsMustExist
#endif
TEST(ExtensionFileUtil, MAYBE_BackgroundScriptsMustExist) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  scoped_ptr<DictionaryValue> value(new DictionaryValue());
  value->SetString("name", "test");
  value->SetString("version", "1");
  value->SetInteger("manifest_version", 1);

  ListValue* scripts = new ListValue();
  scripts->Append(Value::CreateStringValue("foo.js"));
  value->Set("background.scripts", scripts);

  std::string error;
  scoped_refptr<Extension> extension = LoadExtensionManifest(
      value.get(), temp.path(), Extension::LOAD, 0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_FALSE(extension_file_util::ValidateExtension(extension, &error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
      IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED, ASCIIToUTF16("foo.js")),
           error);

  scripts->Clear();
  scripts->Append(Value::CreateStringValue("http://google.com/foo.js"));

  extension = LoadExtensionManifest(value.get(), temp.path(), Extension::LOAD,
                                    0, &error);
  ASSERT_TRUE(extension.get()) << error;

  EXPECT_FALSE(extension_file_util::ValidateExtension(extension, &error));
  EXPECT_EQ(l10n_util::GetStringFUTF8(
      IDS_EXTENSION_LOAD_BACKGROUND_SCRIPT_FAILED,
      ASCIIToUTF16("http://google.com/foo.js")),
           error);
}

// TODO(aa): More tests as motivation allows. Maybe steal some from
// ExtensionService? Many of them could probably be tested here without the
// MessageLoop shenanigans.
