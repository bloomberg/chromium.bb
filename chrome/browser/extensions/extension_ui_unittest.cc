// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop/message_loop.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/extensions/permissions_updater.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/webui/extensions/extension_settings_handler.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "components/crx_file/id_util.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/management_policy.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/value_builder.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

namespace {
const char kAllHostsPermission[] = "*://*/*";
}

class ExtensionUITest : public testing::Test {
 public:
  ExtensionUITest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_thread_(content::BrowserThread::FILE, &message_loop_)  {}

 protected:
  virtual void SetUp() OVERRIDE {
    // Create an ExtensionService and ManagementPolicy to inject into the
    // ExtensionSettingsHandler.
    profile_.reset(new TestingProfile());
    TestExtensionSystem* system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    extension_service_ = system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);
    management_policy_ = system->management_policy();

    handler_.reset(new ExtensionSettingsHandler(extension_service_,
                                                management_policy_));
  }

  virtual void TearDown() OVERRIDE {
    handler_.reset();
    profile_.reset();
    // Execute any pending deletion tasks.
    message_loop_.RunUntilIdle();
  }

  static base::DictionaryValue* DeserializeJSONTestData(
      const base::FilePath& path,
      std::string *error) {
    base::Value* value;

    JSONFileValueSerializer serializer(path);
    value = serializer.Deserialize(NULL, error);

    return static_cast<base::DictionaryValue*>(value);
  }

  const scoped_refptr<const Extension> CreateExtension(
      const std::string& name,
      ListBuilder& permissions) {
    const std::string kId = crx_file::id_util::GenerateId(name);
    scoped_refptr<const Extension> extension =
        ExtensionBuilder().SetManifest(
                               DictionaryBuilder()
                                   .Set("name", name)
                                   .Set("description", "an extension")
                                   .Set("manifest_version", 2)
                                   .Set("version", "1.0.0")
                                   .Set("permissions", permissions))
                          .SetLocation(Manifest::INTERNAL)
                          .SetID(kId)
                          .Build();

    ExtensionRegistry::Get(profile())->AddEnabled(extension);
    PermissionsUpdater(profile()).InitializePermissions(extension.get());
    return extension;
  }

  base::DictionaryValue* CreateExtensionDetailViewFromPath(
      const base::FilePath& extension_path,
      const std::vector<ExtensionPage>& pages,
      Manifest::Location location) {
    std::string error;

    base::FilePath manifest_path = extension_path.Append(kManifestFilename);
    scoped_ptr<base::DictionaryValue> extension_data(DeserializeJSONTestData(
        manifest_path, &error));
    EXPECT_EQ("", error);

    scoped_refptr<Extension> extension(Extension::Create(
        extension_path, location, *extension_data, Extension::REQUIRE_KEY,
        &error));
    EXPECT_TRUE(extension.get());
    EXPECT_EQ("", error);

    return handler_->CreateExtensionDetailValue(extension.get(), pages, NULL);
  }

  void CompareExpectedAndActualOutput(
      const base::FilePath& extension_path,
      const std::vector<ExtensionPage>& pages,
      const base::FilePath& expected_output_path) {
    std::string error;

    scoped_ptr<base::DictionaryValue> expected_output_data(
        DeserializeJSONTestData(expected_output_path, &error));
    EXPECT_EQ("", error);

    // Produce test output.
    scoped_ptr<base::DictionaryValue> actual_output_data(
        CreateExtensionDetailViewFromPath(
            extension_path, pages, Manifest::INVALID_LOCATION));

    // Compare the outputs.
    // Ignore unknown fields in the actual output data.
    std::string paths_details = " - expected (" +
        expected_output_path.MaybeAsASCII() + ") vs. actual (" +
        extension_path.MaybeAsASCII() + ")";
    for (base::DictionaryValue::Iterator field(*expected_output_data);
         !field.IsAtEnd(); field.Advance()) {
      const base::Value* expected_value = &field.value();
      base::Value* actual_value = NULL;
      EXPECT_TRUE(actual_output_data->Get(field.key(), &actual_value)) <<
          field.key() + " is missing" + paths_details;
      EXPECT_TRUE(expected_value->Equals(actual_value)) << field.key() +
          paths_details;
    }
  }

  Profile* profile() { return profile_.get(); }
  ExtensionSettingsHandler* handler() { return handler_.get(); }

  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
  ManagementPolicy* management_policy_;
  scoped_ptr<ExtensionSettingsHandler> handler_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(ExtensionUITest, GenerateExtensionsJSONData) {
  base::FilePath data_test_dir_path, extension_path, expected_output_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_test_dir_path));

  // Test Extension1
  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::vector<ExtensionPage> pages;
  pages.push_back(ExtensionPage(
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/bar.html"),
      42, 88, false, false));
  pages.push_back(ExtensionPage(
      GURL("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/dog.html"),
      0, 0, false, false));

  expected_output_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("ui")
      .AppendASCII("create_extension_detail_value_expected_output")
      .AppendASCII("good-extension1.json");

  CompareExpectedAndActualOutput(extension_path, pages, expected_output_path);

#if !defined(OS_CHROMEOS)
  // Test Extension2
  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("hpiknbiabeeppbpihjehijgoemciehgk")
      .AppendASCII("2");

  expected_output_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("ui")
      .AppendASCII("create_extension_detail_value_expected_output")
      .AppendASCII("good-extension2.json");

  // It's OK to have duplicate URLs, so long as the IDs are different.
  pages[1].url = pages[0].url;

  CompareExpectedAndActualOutput(extension_path, pages, expected_output_path);
#endif

  // Test Extension3
  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("bjafgdebaacbbbecmhlhpofkepfkgcpa")
      .AppendASCII("1.0");

  expected_output_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("ui")
      .AppendASCII("create_extension_detail_value_expected_output")
      .AppendASCII("good-extension3.json");

  pages.clear();

  CompareExpectedAndActualOutput(extension_path, pages, expected_output_path);
}

// Test that using Manifest::UNPACKED for the extension location triggers the
// correct values in the details, including location, order, and allow_reload.
TEST_F(ExtensionUITest, LocationLoadPropagation) {
  base::FilePath data_test_dir_path, extension_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_test_dir_path));

  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::vector<ExtensionPage> pages;

  scoped_ptr<base::DictionaryValue> extension_details(
      CreateExtensionDetailViewFromPath(
          extension_path, pages, Manifest::UNPACKED));

  bool ui_allow_reload = false;
  bool ui_is_unpacked = false;
  base::FilePath::StringType ui_path;

  EXPECT_TRUE(extension_details->GetBoolean("allow_reload", &ui_allow_reload));
  EXPECT_TRUE(extension_details->GetBoolean("isUnpacked", &ui_is_unpacked));
  EXPECT_TRUE(extension_details->GetString("path", &ui_path));
  EXPECT_EQ(true, ui_allow_reload);
  EXPECT_EQ(true, ui_is_unpacked);
  EXPECT_EQ(extension_path, base::FilePath(ui_path));
}

// Test that using Manifest::EXTERNAL_PREF for the extension location triggers
// the correct values in the details, including location, order, and
// allow_reload.  Contrast to Manifest::UNPACKED, which has somewhat different
// values.
TEST_F(ExtensionUITest, LocationExternalPrefPropagation) {
  base::FilePath data_test_dir_path, extension_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_test_dir_path));

  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::vector<ExtensionPage> pages;

  scoped_ptr<base::DictionaryValue> extension_details(
      CreateExtensionDetailViewFromPath(
          extension_path, pages, Manifest::EXTERNAL_PREF));

  bool ui_allow_reload = true;
  bool ui_is_unpacked = true;
  base::FilePath::StringType ui_path;

  EXPECT_TRUE(extension_details->GetBoolean("allow_reload", &ui_allow_reload));
  EXPECT_TRUE(extension_details->GetBoolean("isUnpacked", &ui_is_unpacked));
  EXPECT_FALSE(extension_details->GetString("path", &ui_path));
  EXPECT_FALSE(ui_allow_reload);
  EXPECT_FALSE(ui_is_unpacked);
}

// Test that the extension path is correctly propagated into the extension
// details.
TEST_F(ExtensionUITest, PathPropagation) {
  base::FilePath data_test_dir_path, extension_path;
  EXPECT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &data_test_dir_path));

  extension_path = data_test_dir_path.AppendASCII("extensions")
      .AppendASCII("good")
      .AppendASCII("Extensions")
      .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
      .AppendASCII("1.0.0.0");

  std::vector<ExtensionPage> pages;

  scoped_ptr<base::DictionaryValue> extension_details(
      CreateExtensionDetailViewFromPath(
          extension_path, pages, Manifest::UNPACKED));

  base::FilePath::StringType ui_path;

  EXPECT_TRUE(extension_details->GetString("path", &ui_path));
  EXPECT_EQ(extension_path, base::FilePath(ui_path));
}

// Test that the all_urls checkbox only shows up for extensions that want all
// urls, and only when the switch is on.
TEST_F(ExtensionUITest, ExtensionUIAllUrlsCheckbox) {
  // Start with the switch enabled.
  scoped_ptr<FeatureSwitch::ScopedOverride> enable_scripts_switch(
      new FeatureSwitch::ScopedOverride(
          FeatureSwitch::scripts_require_action(), true));
  // Two extensions - one with all urls, one without.
  scoped_refptr<const Extension> all_urls_extension = CreateExtension(
      "all_urls", ListBuilder().Append(kAllHostsPermission).Pass());
  scoped_refptr<const Extension> no_urls_extension =
      CreateExtension("no urls", ListBuilder().Pass());

  scoped_ptr<base::DictionaryValue> value(handler()->CreateExtensionDetailValue(
      all_urls_extension.get(), std::vector<ExtensionPage>(), NULL));
  bool result = false;
  const std::string kWantsAllUrls = "wantsAllUrls";
  const std::string kAllowAllUrls = "allowAllUrls";

  // The extension should want all urls, but not currently have it.
  EXPECT_TRUE(value->GetBoolean(kWantsAllUrls, &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(value->GetBoolean(kAllowAllUrls, &result));
  EXPECT_FALSE(result);

  // Give the extension all urls.
  util::SetAllowedScriptingOnAllUrls(
      all_urls_extension->id(), profile(), true);

  // Now the extension should both want and have all urls.
  value.reset(handler()->CreateExtensionDetailValue(
      all_urls_extension.get(), std::vector<ExtensionPage>(), NULL));
  EXPECT_TRUE(value->GetBoolean(kWantsAllUrls, &result));
  EXPECT_TRUE(result);
  EXPECT_TRUE(value->GetBoolean(kAllowAllUrls, &result));
  EXPECT_TRUE(result);

  // The other extension should neither want nor have all urls.
  value.reset(handler()->CreateExtensionDetailValue(
      no_urls_extension.get(), std::vector<ExtensionPage>(), NULL));
  EXPECT_TRUE(value->GetBoolean(kWantsAllUrls, &result));
  EXPECT_FALSE(result);
  EXPECT_TRUE(value->GetBoolean(kAllowAllUrls, &result));
  EXPECT_FALSE(result);

  // Turn off the switch and load another extension (so permissions are
  // re-initialized).
  enable_scripts_switch.reset();

  // Even though the extension has the all urls preference, the checkbox
  // shouldn't show up with the switch off.
  value.reset(handler()->CreateExtensionDetailValue(
      all_urls_extension.get(), std::vector<ExtensionPage>(), NULL));
  EXPECT_TRUE(value->GetBoolean(kWantsAllUrls, &result));
  EXPECT_FALSE(result);
  EXPECT_TRUE(value->GetBoolean(kAllowAllUrls, &result));
  EXPECT_TRUE(result);

  // Load another extension with all urls (so permissions get re-init'd).
  all_urls_extension = CreateExtension(
      "all_urls_II", ListBuilder().Append(kAllHostsPermission).Pass());

  // Even though the extension has all_urls permission, the checkbox shouldn't
  // show up without the switch.
  value.reset(handler()->CreateExtensionDetailValue(
      all_urls_extension.get(), std::vector<ExtensionPage>(), NULL));
  EXPECT_TRUE(value->GetBoolean(kWantsAllUrls, &result));
  EXPECT_FALSE(result);
  EXPECT_TRUE(value->GetBoolean(kAllowAllUrls, &result));
  EXPECT_FALSE(result);
}

}  // namespace extensions
