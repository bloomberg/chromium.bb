// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_l10n_util.h"

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

namespace {

  Extension* CreateMinimalExtension(const std::string& default_locale) {
#if defined(OS_WIN)
  FilePath path(FILE_PATH_LITERAL("C:\\foo"));
#elif defined(OS_POSIX)
  FilePath path(FILE_PATH_LITERAL("/foo"));
#endif
  Extension* extension = new Extension(path);
  std::string error;
  DictionaryValue input_value;

  // Test minimal extension
  input_value.SetString(keys::kVersion, "1.0.0.0");
  input_value.SetString(keys::kName, "my extension");
  if (!default_locale.empty()) {
    input_value.SetString(keys::kDefaultLocale, default_locale);
  }
  EXPECT_TRUE(extension->InitFromValue(input_value, false, &error));

  return extension;
}

TEST(ExtensionL10nUtil, AddValidLocalesEmptyLocaleFolder) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  scoped_ptr<Extension> extension(CreateMinimalExtension(""));

  std::string error;
  EXPECT_FALSE(extension_l10n_util::AddValidLocales(src_path,
                                                    extension.get(),
                                                    &error));

  EXPECT_TRUE(extension->supported_locales().empty());
}

TEST(ExtensionL10nUtil, AddValidLocalesWithValidLocaleNoMessagesFile) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

  FilePath src_path = temp.path().AppendASCII(Extension::kLocaleFolder);
  ASSERT_TRUE(file_util::CreateDirectory(src_path));

  ASSERT_TRUE(file_util::CreateDirectory(src_path.AppendASCII("sr")));

  scoped_ptr<Extension> extension(CreateMinimalExtension(""));

  std::string error;
  EXPECT_FALSE(extension_l10n_util::AddValidLocales(src_path,
                                                    extension.get(),
                                                    &error));

  EXPECT_TRUE(extension->supported_locales().empty());
}

TEST(ExtensionL10nUtil, AddValidLocalesWithValidLocalesAndMessagesFile) {
  ScopedTempDir temp;
  ASSERT_TRUE(temp.CreateUniqueTempDir());

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

  scoped_ptr<Extension> extension(CreateMinimalExtension(""));

  std::string error;
  EXPECT_TRUE(extension_l10n_util::AddValidLocales(src_path,
                                                   extension.get(),
                                                   &error));

  EXPECT_EQ(static_cast<unsigned int>(2),
            extension->supported_locales().size());
}

TEST(ExtensionL10nUtil, SetDefaultLocaleGoodDefaultLocaleInManifest) {
  scoped_ptr<Extension> extension(CreateMinimalExtension("sr"));
  extension->AddSupportedLocale("sr");
  extension->AddSupportedLocale("en-US");

  EXPECT_TRUE(extension_l10n_util::ValidateDefaultLocale(extension.get()));
  EXPECT_EQ("sr", extension->default_locale());
}

TEST(ExtensionL10nUtil, SetDefaultLocaleNoDefaultLocaleInManifest) {
  scoped_ptr<Extension> extension(CreateMinimalExtension(""));
  extension->AddSupportedLocale("sr");
  extension->AddSupportedLocale("en-US");

  EXPECT_FALSE(extension_l10n_util::ValidateDefaultLocale(extension.get()));
}

TEST(ExtensionL10nUtil, SetDefaultLocaleWrongDefaultLocaleInManifest) {
  scoped_ptr<Extension> extension(CreateMinimalExtension("ko"));
  extension->AddSupportedLocale("sr");
  extension->AddSupportedLocale("en-US");

  EXPECT_FALSE(extension_l10n_util::ValidateDefaultLocale(extension.get()));
}

}  // namespace
