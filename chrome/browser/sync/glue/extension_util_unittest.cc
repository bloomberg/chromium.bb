// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_util.h"

#include "base/file_path.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_sync_data.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

#if defined(OS_WIN)
const FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("c:\\foo");
#elif defined(OS_POSIX)
const FilePath::CharType kExtensionFilePath[] = FILE_PATH_LITERAL("/foo");
#endif

const char kValidId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kValidVersion[] = "0.0.0.0";
const char kVersion1[] = "1.0.0.1";
const char kVersion2[] = "1.0.1.0";
const char kVersion3[] = "1.1.0.0";
const char kValidUpdateUrl1[] =
    "http://clients2.google.com/service/update2/crx";
const char kValidUpdateUrl2[] =
    "https://clients2.google.com/service/update2/crx";
const char kName[] = "MyExtension";
const char kName2[] = "MyExtension2";

class ExtensionUtilTest : public testing::Test {
};

scoped_refptr<Extension> MakeExtension(
    bool is_theme, const GURL& update_url,
    const GURL& launch_url,
    bool converted_from_user_script,
    Extension::Location location, int num_plugins,
    const FilePath& extension_path) {
  DictionaryValue source;
  source.SetString(extension_manifest_keys::kName,
                   "PossiblySyncableExtension");
  source.SetString(extension_manifest_keys::kVersion, "0.0.0.0");
  if (is_theme) {
    source.Set(extension_manifest_keys::kTheme, new DictionaryValue());
  }
  if (!update_url.is_empty()) {
    source.SetString(extension_manifest_keys::kUpdateURL,
                     update_url.spec());
  }
  if (!launch_url.is_empty()) {
    source.SetString(extension_manifest_keys::kLaunchWebURL,
                     launch_url.spec());
  }
  if (!is_theme) {
    source.SetBoolean(extension_manifest_keys::kConvertedFromUserScript,
                      converted_from_user_script);
    ListValue* plugins = new ListValue();
    for (int i = 0; i < num_plugins; ++i) {
      DictionaryValue* plugin = new DictionaryValue();
      plugin->SetString(extension_manifest_keys::kPluginsPath, "");
      plugins->Set(i, plugin);
    }
    source.Set(extension_manifest_keys::kPlugins, plugins);
  }

  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      extension_path, location, source, Extension::STRICT_ERROR_CHECKS, &error);
  EXPECT_TRUE(extension);
  EXPECT_EQ("", error);
  return extension;
}

TEST_F(ExtensionUtilTest, IsExtensionValid) {
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(), GURL(), false,
                      Extension::INTERNAL, 0, file_path));
    EXPECT_TRUE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(kValidUpdateUrl1), GURL(),
                      true, Extension::INTERNAL, 0, file_path));
    EXPECT_TRUE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(), GURL(), true,
                      Extension::INTERNAL, 0, file_path));
    EXPECT_TRUE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(true, GURL(), GURL(), false,
                      Extension::INTERNAL, 0, file_path));
    EXPECT_TRUE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(),
                      GURL("http://www.google.com"), false,
                      Extension::INTERNAL, 0, file_path));
    EXPECT_TRUE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(), GURL(), false,
                      Extension::EXTERNAL_PREF, 0, file_path));
    EXPECT_FALSE(IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(
            false, GURL("http://third-party.update_url.com"), GURL(), true,
            Extension::INTERNAL, 0, file_path));
    EXPECT_FALSE(IsExtensionValid(*extension));
  }
  // These last 2 tests don't make sense on Chrome OS, where extension plugins
  // are not allowed.
#if !defined(OS_CHROMEOS)
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(), GURL(), true,
                      Extension::INTERNAL, 1, file_path));
    EXPECT_FALSE(extension && IsExtensionValid(*extension));
  }
  {
    FilePath file_path(kExtensionFilePath);
    scoped_refptr<Extension> extension(
        MakeExtension(false, GURL(), GURL(), true,
                      Extension::INTERNAL, 2, file_path));
    EXPECT_FALSE(extension && IsExtensionValid(*extension));
  }
#endif
}

TEST_F(ExtensionUtilTest, SpecificsToSyncData) {
  sync_pb::ExtensionSpecifics specifics;
  specifics.set_id(kValidId);
  specifics.set_update_url(kValidUpdateUrl2);
  specifics.set_enabled(false);
  specifics.set_incognito_enabled(true);
  specifics.set_version(kVersion1);
  specifics.set_name(kName);

  ExtensionSyncData sync_data;
  EXPECT_TRUE(SpecificsToSyncData(specifics, &sync_data));
  EXPECT_EQ(specifics.id(), sync_data.id);
  EXPECT_EQ(specifics.version(), sync_data.version.GetString());
  EXPECT_EQ(specifics.update_url(), sync_data.update_url.spec());
  EXPECT_EQ(specifics.enabled(), sync_data.enabled);
  EXPECT_EQ(specifics.incognito_enabled(), sync_data.incognito_enabled);
  EXPECT_EQ(specifics.name(), sync_data.name);
}

TEST_F(ExtensionUtilTest, SyncDataToSpecifics) {
  ExtensionSyncData sync_data;
  sync_data.id = kValidId;
  sync_data.update_url = GURL(kValidUpdateUrl2);
  sync_data.enabled = true;
  sync_data.incognito_enabled = false;
  {
    scoped_ptr<Version> version(Version::GetVersionFromString(kVersion1));
    sync_data.version = *version;
  }
  sync_data.name = kName;

  sync_pb::ExtensionSpecifics specifics;
  SyncDataToSpecifics(sync_data, &specifics);
  EXPECT_EQ(sync_data.id, specifics.id());
  EXPECT_EQ(sync_data.update_url.spec(), specifics.update_url());
  EXPECT_EQ(sync_data.enabled, specifics.enabled());
  EXPECT_EQ(sync_data.incognito_enabled, specifics.incognito_enabled());
  EXPECT_EQ(sync_data.version.GetString(), specifics.version());
  EXPECT_EQ(sync_data.name, specifics.name());
}

}  // namespace

}  // namespace browser_sync
