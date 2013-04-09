// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/common/extensions/api/plugins/plugins_handler.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/extension_unittest.h"
#include "chrome/common/extensions/manifest.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "googleurl/src/gurl.h"

namespace keys = extension_manifest_keys;
namespace errors = extension_manifest_errors;

namespace extensions {

class ExtensionSyncTypeTest : public ExtensionTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ExtensionTest::SetUp();
    (new UpdateURLHandler)->Register();
    (new PluginsHandler)->Register();
  }

  enum SyncTestExtensionType {
    EXTENSION,
    APP,
    USER_SCRIPT,
    THEME
  };

  static scoped_refptr<Extension> MakeSyncTestExtension(
      SyncTestExtensionType type,
      const GURL& update_url,
      const GURL& launch_url,
      Manifest::Location location,
      int num_plugins,
      const base::FilePath& extension_path,
      int creation_flags) {
    DictionaryValue source;
    source.SetString(keys::kName, "PossiblySyncableExtension");
    source.SetString(keys::kVersion, "0.0.0.0");
    if (type == APP)
      source.SetString(keys::kApp, "true");
    if (type == THEME)
      source.Set(keys::kTheme, new DictionaryValue());
    if (!update_url.is_empty()) {
      source.SetString(keys::kUpdateURL, update_url.spec());
    }
    if (!launch_url.is_empty()) {
      source.SetString(keys::kLaunchWebURL, launch_url.spec());
    }
    if (type != THEME) {
      source.SetBoolean(keys::kConvertedFromUserScript, type == USER_SCRIPT);
      ListValue* plugins = new ListValue();
      for (int i = 0; i < num_plugins; ++i) {
        DictionaryValue* plugin = new DictionaryValue();
        plugin->SetString(keys::kPluginsPath, "");
        plugins->Set(i, plugin);
      }
      source.Set(keys::kPlugins, plugins);
    }

    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        extension_path, location, source, creation_flags, &error);
    EXPECT_TRUE(extension);
    EXPECT_EQ("", error);
    return extension;
  }

  static const char kValidUpdateUrl1[];
  static const char kValidUpdateUrl2[];
};

const char ExtensionSyncTypeTest::kValidUpdateUrl1[] =
    "http://clients2.google.com/service/update2/crx";
const char ExtensionSyncTypeTest::kValidUpdateUrl2[] =
    "https://clients2.google.com/service/update2/crx";

TEST_F(ExtensionSyncTypeTest, NormalExtensionNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, GetSyncTypeUserScriptValidUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(kValidUpdateUrl1), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, UserScriptNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(USER_SCRIPT, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, ThemeNoUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(THEME, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, ExtensionWithLaunchUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL("http://www.google.com"),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_NE(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, ExtensionExternal) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::EXTERNAL_PREF, 0, base::FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, UserScriptThirdPartyUpdateUrl) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(
          USER_SCRIPT, GURL("http://third-party.update_url.com"), GURL(),
          Manifest::INTERNAL, 0, base::FilePath(), Extension::NO_FLAGS));
  EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, OnlyDisplayAppsInLauncher) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));

  EXPECT_FALSE(extension->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(extension->ShouldDisplayInNewTabPage());

  scoped_refptr<Extension> app(
      MakeSyncTestExtension(APP, GURL(), GURL("http://www.google.com"),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());
}

TEST_F(ExtensionSyncTypeTest, DisplayInXManifestProperties) {
  DictionaryValue manifest;
  manifest.SetString(keys::kName, "TestComponentApp");
  manifest.SetString(keys::kVersion, "0.0.0.0");
  manifest.SetString(keys::kApp, "true");
  manifest.SetString(keys::kPlatformAppBackgroundPage, "");

  std::string error;
  scoped_refptr<Extension> app;

  // Default to true.
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP defaults to display_in_launcher.
  manifest.SetBoolean(keys::kDisplayInLauncher, false);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(app->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP = true overriding display_in_launcher = false.
  manifest.SetBoolean(keys::kDisplayInNewTabPage, true);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_FALSE(app->ShouldDisplayInAppLauncher());
  EXPECT_TRUE(app->ShouldDisplayInNewTabPage());

  // Value display_in_NTP = false only, overrides default = true.
  manifest.Remove(keys::kDisplayInLauncher, NULL);
  manifest.SetBoolean(keys::kDisplayInNewTabPage, false);
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string());
  EXPECT_TRUE(app->ShouldDisplayInAppLauncher());
  EXPECT_FALSE(app->ShouldDisplayInNewTabPage());

  // Error checking.
  manifest.SetString(keys::kDisplayInNewTabPage, "invalid");
  app = Extension::Create(
      base::FilePath(), Manifest::COMPONENT, manifest, 0, &error);
  EXPECT_EQ(error, std::string(errors::kInvalidDisplayInNewTabPage));
}

TEST_F(ExtensionSyncTypeTest, OnlySyncInternal) {
  scoped_refptr<Extension> extension_internal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_TRUE(extension_internal->IsSyncable());

  scoped_refptr<Extension> extension_noninternal(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::COMPONENT, 0, base::FilePath(),
                            Extension::NO_FLAGS));
  EXPECT_FALSE(extension_noninternal->IsSyncable());
}

TEST_F(ExtensionSyncTypeTest, DontSyncDefault) {
  scoped_refptr<Extension> extension_default(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 0, base::FilePath(),
                            Extension::WAS_INSTALLED_BY_DEFAULT));
  EXPECT_FALSE(extension_default->IsSyncable());
}

// These last 2 tests don't make sense on Chrome OS, where extension plugins
// are not allowed.
#if !defined(OS_CHROMEOS)
TEST_F(ExtensionSyncTypeTest, ExtensionWithPlugin) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 1, base::FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}

TEST_F(ExtensionSyncTypeTest, ExtensionWithTwoPlugins) {
  scoped_refptr<Extension> extension(
      MakeSyncTestExtension(EXTENSION, GURL(), GURL(),
                            Manifest::INTERNAL, 2, base::FilePath(),
                            Extension::NO_FLAGS));
  if (extension)
    EXPECT_EQ(extension->GetSyncType(), Extension::SYNC_TYPE_NONE);
}
#endif // !defined(OS_CHROMEOS)

}  // namespace extensions
