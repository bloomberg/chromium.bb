// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_util.h"

#include "base/file_path.h"
#include "base/values.h"
#include "chrome/browser/sync/protocol/extension_specifics.pb.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
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
      extension_path, location, source, false, true, &error);
#if defined(OS_CHROMEOS)
  if (num_plugins > 0) {  // plugins are illegal in extensions on chrome os.
    EXPECT_FALSE(extension);
    return NULL;
  }
#endif
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
}

TEST_F(ExtensionUtilTest, IsExtensionSpecificsUnset) {
  {
    sync_pb::ExtensionSpecifics specifics;
    EXPECT_TRUE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_id("a");
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_version("a");
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_update_url("a");
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_enabled(true);
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_incognito_enabled(true);
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }

  {
    sync_pb::ExtensionSpecifics specifics;
    specifics.set_name("a");
    EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  }
}

TEST_F(ExtensionUtilTest, IsExtensionSpecificsValid) {
  sync_pb::ExtensionSpecifics specifics;
  EXPECT_FALSE(IsExtensionSpecificsValid(specifics));
  specifics.set_id(kValidId);
  EXPECT_FALSE(IsExtensionSpecificsValid(specifics));
  specifics.set_version(kValidVersion);
  EXPECT_TRUE(IsExtensionSpecificsValid(specifics));
  EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));
  specifics.set_update_url(kValidUpdateUrl1);
  EXPECT_TRUE(IsExtensionSpecificsValid(specifics));
  EXPECT_FALSE(IsExtensionSpecificsUnset(specifics));

  {
    sync_pb::ExtensionSpecifics specifics_copy(specifics);
    specifics_copy.set_id("invalid");
    EXPECT_FALSE(IsExtensionSpecificsValid(specifics_copy));
  }

  {
    sync_pb::ExtensionSpecifics specifics_copy(specifics);
    specifics_copy.set_version("invalid");
    EXPECT_FALSE(IsExtensionSpecificsValid(specifics_copy));
  }

  {
    sync_pb::ExtensionSpecifics specifics_copy(specifics);
    specifics_copy.set_update_url("http:invalid.com:invalid");
    EXPECT_FALSE(IsExtensionSpecificsValid(specifics_copy));
  }
}

TEST_F(ExtensionUtilTest, AreExtensionSpecificsEqual) {
  sync_pb::ExtensionSpecifics a, b;
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_id("a");
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_id("a");
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_version("1.5");
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_version("1.5");
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_update_url("http://www.foo.com");
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_update_url("http://www.foo.com");
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_enabled(true);
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_enabled(true);
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_incognito_enabled(true);
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_incognito_enabled(true);
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));

  a.set_name("name");
  EXPECT_FALSE(AreExtensionSpecificsEqual(a, b));
  b.set_name("name");
  EXPECT_TRUE(AreExtensionSpecificsEqual(a, b));
}

TEST_F(ExtensionUtilTest, CopyUserProperties) {
  sync_pb::ExtensionSpecifics dest_specifics;
  dest_specifics.set_version(kVersion2);
  dest_specifics.set_update_url(kValidUpdateUrl1);
  dest_specifics.set_enabled(true);
  dest_specifics.set_incognito_enabled(false);
  dest_specifics.set_name(kName);

  sync_pb::ExtensionSpecifics specifics;
  specifics.set_id(kValidId);
  specifics.set_version(kVersion3);
  specifics.set_update_url(kValidUpdateUrl2);
  specifics.set_enabled(false);
  specifics.set_incognito_enabled(true);
  specifics.set_name(kName2);

  CopyUserProperties(specifics, &dest_specifics);
  EXPECT_EQ("", dest_specifics.id());
  EXPECT_EQ(kVersion2, dest_specifics.version());
  EXPECT_EQ(kValidUpdateUrl1, dest_specifics.update_url());
  EXPECT_FALSE(dest_specifics.enabled());
  EXPECT_TRUE(dest_specifics.incognito_enabled());
  EXPECT_EQ(kName, dest_specifics.name());
}

TEST_F(ExtensionUtilTest, CopyNonUserProperties) {
  sync_pb::ExtensionSpecifics dest_specifics;
  dest_specifics.set_id(kValidId);
  dest_specifics.set_version(kVersion2);
  dest_specifics.set_update_url(kValidUpdateUrl1);
  dest_specifics.set_enabled(true);
  dest_specifics.set_incognito_enabled(false);
  dest_specifics.set_name(kName);

  sync_pb::ExtensionSpecifics specifics;
  specifics.set_id("");
  specifics.set_version(kVersion3);
  specifics.set_update_url(kValidUpdateUrl2);
  specifics.set_enabled(false);
  specifics.set_incognito_enabled(true);
  specifics.set_name(kName2);

  CopyNonUserProperties(specifics, &dest_specifics);
  EXPECT_EQ("", dest_specifics.id());
  EXPECT_EQ(kVersion3, dest_specifics.version());
  EXPECT_EQ(kValidUpdateUrl2, dest_specifics.update_url());
  EXPECT_TRUE(dest_specifics.enabled());
  EXPECT_FALSE(dest_specifics.incognito_enabled());
  EXPECT_EQ(kName2, dest_specifics.name());
}

TEST_F(ExtensionUtilTest, AreExtensionSpecificsUserPropertiesEqual) {
  sync_pb::ExtensionSpecifics a, b;
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_id("a");
  b.set_id("b");
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_version("1.5");
  b.set_version("1.6");
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_name("name");
  b.set_name("name2");
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_update_url("http://www.foo.com");
  b.set_update_url("http://www.foo2.com");
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_enabled(true);
  EXPECT_FALSE(AreExtensionSpecificsUserPropertiesEqual(a, b));
  b.set_enabled(true);
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));

  a.set_incognito_enabled(true);
  EXPECT_FALSE(AreExtensionSpecificsUserPropertiesEqual(a, b));
  b.set_incognito_enabled(true);
  EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(a, b));
}

TEST_F(ExtensionUtilTest, AreExtensionSpecificsNonUserPropertiesEqual) {
  sync_pb::ExtensionSpecifics a, b;
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_enabled(true);
  b.set_enabled(false);
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_incognito_enabled(true);
  b.set_incognito_enabled(false);
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_id("a");
  EXPECT_FALSE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));
  b.set_id("a");
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_version("1.5");
  EXPECT_FALSE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));
  b.set_version("1.5");
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_update_url("http://www.foo.com");
  EXPECT_FALSE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));
  b.set_update_url("http://www.foo.com");
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));

  a.set_name("name");
  EXPECT_FALSE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));
  b.set_name("name");
  EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(a, b));
}

scoped_refptr<Extension> MakeSyncableExtension(
    const std::string& version_string,
    const std::string& update_url_spec,
    const std::string& name,
    const FilePath& extension_path) {
  DictionaryValue source;
  source.SetString(extension_manifest_keys::kVersion, version_string);
  source.SetString(extension_manifest_keys::kUpdateURL, update_url_spec);
  source.SetString(extension_manifest_keys::kName, name);
  std::string error;
  scoped_refptr<Extension> extension = Extension::Create(
      extension_path, Extension::INTERNAL, source, false, true, &error);
  EXPECT_TRUE(extension);
  EXPECT_EQ("", error);
  return extension;
}

TEST_F(ExtensionUtilTest, GetExtensionSpecificsHelper) {
  FilePath file_path(kExtensionFilePath);
  scoped_refptr<Extension> extension(
      MakeSyncableExtension(kValidVersion, kValidUpdateUrl1, kName, file_path));
  sync_pb::ExtensionSpecifics specifics;
  GetExtensionSpecificsHelper(*extension, true, false, &specifics);
  EXPECT_EQ(extension->id(), specifics.id());
  EXPECT_EQ(extension->VersionString(), kValidVersion);
  EXPECT_EQ(extension->update_url().spec(), kValidUpdateUrl1);
  EXPECT_TRUE(specifics.enabled());
  EXPECT_FALSE(specifics.incognito_enabled());
  EXPECT_EQ(kName, specifics.name());
}

TEST_F(ExtensionUtilTest, IsExtensionOutdated) {
  FilePath file_path(kExtensionFilePath);
  scoped_refptr<Extension> extension(
      MakeSyncableExtension(kVersion2, kValidUpdateUrl1, kName, file_path));
  sync_pb::ExtensionSpecifics specifics;
  specifics.set_id(kValidId);
  specifics.set_update_url(kValidUpdateUrl1);

  specifics.set_version(kVersion1);
  EXPECT_FALSE(IsExtensionOutdated(*extension, specifics));
  specifics.set_version(kVersion2);
  EXPECT_FALSE(IsExtensionOutdated(*extension, specifics));
  specifics.set_version(kVersion3);
  EXPECT_TRUE(IsExtensionOutdated(*extension, specifics));
}

// TODO(akalin): Make ExtensionService/ExtensionUpdater testable
// enough to be able to write a unittest for SetExtensionProperties().

TEST_F(ExtensionUtilTest, MergeExtensionSpecificsWithUserProperties) {
  sync_pb::ExtensionSpecifics merged_specifics;
  merged_specifics.set_id(kValidId);
  merged_specifics.set_update_url(kValidUpdateUrl1);
  merged_specifics.set_enabled(true);
  merged_specifics.set_incognito_enabled(false);
  merged_specifics.set_version(kVersion2);

  sync_pb::ExtensionSpecifics specifics;
  specifics.set_id(kValidId);
  specifics.set_update_url(kValidUpdateUrl2);
  merged_specifics.set_enabled(false);
  merged_specifics.set_incognito_enabled(true);

  specifics.set_version(kVersion1);
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, false, &result);
    EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(
        result, merged_specifics));
    EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(
        result, merged_specifics));
  }
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, true, &result);
    EXPECT_TRUE(AreExtensionSpecificsEqual(result, merged_specifics));
  }

  specifics.set_version(kVersion2);
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, false, &result);
    EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(
        result, merged_specifics));
    EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(
        result, specifics));
  }
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, true, &result);
    EXPECT_TRUE(AreExtensionSpecificsEqual(result, specifics));
  }

  specifics.set_version(kVersion3);
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, false, &result);
    EXPECT_TRUE(AreExtensionSpecificsUserPropertiesEqual(
        result, merged_specifics));
    EXPECT_TRUE(AreExtensionSpecificsNonUserPropertiesEqual(
        result, specifics));
  }
  {
    sync_pb::ExtensionSpecifics result = merged_specifics;
    MergeExtensionSpecifics(specifics, true, &result);
    EXPECT_TRUE(AreExtensionSpecificsEqual(result, specifics));
  }
}

}  // namespace

}  // namespace browser_sync
