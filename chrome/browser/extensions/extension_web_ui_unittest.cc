// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_web_ui.h"
#include "chrome/browser/extensions/extension_web_ui_override_registrar.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

namespace {

scoped_ptr<KeyedService> BuildOverrideRegistrar(
    content::BrowserContext* context) {
  return make_scoped_ptr(new ExtensionWebUIOverrideRegistrar(context));
}

}  // namespace

class ExtensionWebUITest : public testing::Test {
 public:
  ExtensionWebUITest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {}

 protected:
  void SetUp() override {
    profile_.reset(new TestingProfile());
    TestExtensionSystem* system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_.get()));
    extension_service_ = system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->SetTestingFactory(
        profile_.get(), &BuildOverrideRegistrar);
    ExtensionWebUIOverrideRegistrar::GetFactoryInstance()->Get(profile_.get());
  }

  void TearDown() override {
    profile_.reset();
    message_loop_.RunUntilIdle();
  }

  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

// Test that component extension url overrides have lower priority than
// non-component extension url overrides.
TEST_F(ExtensionWebUITest, ExtensionURLOverride) {
  const char kOverrideResource[] = "1.html";
  // Register a non-component extension.
  DictionaryBuilder manifest;
  manifest.Set(manifest_keys::kName, "ext1")
      .Set(manifest_keys::kVersion, "0.1")
      .Set(std::string(manifest_keys::kChromeURLOverrides),
           std::move(DictionaryBuilder().Set("bookmarks", kOverrideResource)));
  scoped_refptr<Extension> ext_unpacked(
      ExtensionBuilder()
          .SetManifest(std::move(manifest))
          .SetLocation(Manifest::UNPACKED)
          .SetID("abcdefghijabcdefghijabcdefghijaa")
          .Build());
  extension_service_->AddExtension(ext_unpacked.get());

  const GURL kExpectedUnpackedOverrideUrl =
      ext_unpacked->GetResourceURL(kOverrideResource);
  const GURL kBookmarksUrl("chrome://bookmarks");
  GURL changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  GURL url_plus_fragment = kBookmarksUrl.Resolve("#1");
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverride(&url_plus_fragment,
                                                      profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl.Resolve("#1"),
            url_plus_fragment);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&url_plus_fragment,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl.Resolve("#1"), url_plus_fragment);

  // Register a component extension
  const char kOverrideResource2[] = "2.html";
  DictionaryBuilder manifest2;
  manifest2.Set(manifest_keys::kName, "ext2")
      .Set(manifest_keys::kVersion, "0.1")
      .Set(std::string(manifest_keys::kChromeURLOverrides),
           std::move(DictionaryBuilder().Set("bookmarks", kOverrideResource2)));
  scoped_refptr<Extension> ext_component(
      ExtensionBuilder()
          .SetManifest(std::move(manifest2))
          .SetLocation(Manifest::COMPONENT)
          .SetID("bbabcdefghijabcdefghijabcdefghij")
          .Build());
  extension_service_->AddComponentExtension(ext_component.get());

  // Despite being registered more recently, the component extension should
  // not take precedence over the non-component extension.
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  GURL kExpectedComponentOverrideUrl =
      ext_component->GetResourceURL(kOverrideResource2);

  // Unregister non-component extension. Only component extension remaining.
  ExtensionWebUI::UnregisterChromeURLOverrides(
      profile_.get(), URLOverrides::GetChromeURLOverrides(ext_unpacked.get()));
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedComponentOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);

  // This time the non-component extension was registered more recently and
  // should still take precedence.
  ExtensionWebUI::RegisterOrActivateChromeURLOverrides(
      profile_.get(), URLOverrides::GetChromeURLOverrides(ext_unpacked.get()));
  changed_url = kBookmarksUrl;
  EXPECT_TRUE(
      ExtensionWebUI::HandleChromeURLOverride(&changed_url, profile_.get()));
  EXPECT_EQ(kExpectedUnpackedOverrideUrl, changed_url);
  EXPECT_TRUE(ExtensionWebUI::HandleChromeURLOverrideReverse(&changed_url,
                                                             profile_.get()));
  EXPECT_EQ(kBookmarksUrl, changed_url);
}

}  // namespace extensions
