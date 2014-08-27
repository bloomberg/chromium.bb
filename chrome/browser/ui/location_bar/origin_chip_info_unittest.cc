// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/location_bar/origin_chip_info.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/toolbar/test_toolbar_model.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_builder.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace {

const char kExampleUrl[] = "http://www.example.com/";
const char kExampleUrlSecure[] = "https://www.example.com/";
const char kOtherUrl[] = "http://chrome.google.com/";

}  // namespace

class OriginChipInfoTest : public ChromeRenderViewHostTestHarness,
                           public extensions::IconImage::Observer {
 public:
  OriginChipInfoTest() : icon_image_(NULL) {}

  TestToolbarModel* toolbar_model() { return toolbar_model_.get(); }
  OriginChipInfo* info() { return info_.get(); }
  const GURL& url() const { return url_; }
  const extensions::IconImage* icon_image() const { return icon_image_; }

  void SetURL(const std::string& dest_url, bool expect_update) {
    url_ = GURL(dest_url);
    NavigateAndCommit(url_);
    toolbar_model_->set_url(url_);

    EXPECT_EQ(expect_update, info_->Update(web_contents(), toolbar_model()));
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
#if defined(OS_CHROMEOS)
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    toolbar_model_.reset(new TestToolbarModel());
    info_.reset(new OriginChipInfo(this, profile()));
  }

  virtual void TearDown() OVERRIDE {
    info_.reset();
    toolbar_model_.reset();
#if defined(OS_CHROMEOS)
    test_user_manager_.reset();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
  }

  virtual void OnExtensionIconImageChanged(
      extensions::IconImage* image) OVERRIDE {
    // We keep the value of |image| to check if it's set, but the actual value
    // is never used.
    icon_image_ = image;
  }

 private:
  scoped_ptr<OriginChipInfo> info_;
  scoped_ptr<TestToolbarModel> toolbar_model_;
  GURL url_;
  extensions::IconImage* icon_image_;

#if defined(OS_CHROMEOS)
  // OriginChipInfo sometimes calls into the extensions system, which, on CrOS,
  // requires these services to be initialized.
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(OriginChipInfoTest);
};

TEST_F(OriginChipInfoTest, NoChangeShouldNotUpdate) {
  SetURL(kExampleUrl, true);
  SetURL(kExampleUrl, false);
}

TEST_F(OriginChipInfoTest, ChangeShouldUpdate) {
  SetURL(kExampleUrl, true);
  SetURL(kOtherUrl, true);
}

TEST_F(OriginChipInfoTest, NormalOrigin) {
  SetURL(kExampleUrl, true);

  EXPECT_EQ(base::ASCIIToUTF16("example.com"), info()->label());
  EXPECT_EQ(base::ASCIIToUTF16(kExampleUrl), info()->Tooltip());
  EXPECT_EQ(url(), info()->displayed_url());
  EXPECT_EQ(ToolbarModel::NONE, info()->security_level());
}

TEST_F(OriginChipInfoTest, EVSecureOrigin) {
  toolbar_model()->set_security_level(ToolbarModel::EV_SECURE);
  toolbar_model()->set_ev_cert_name(base::ASCIIToUTF16("Example [US]"));
  SetURL(kExampleUrlSecure, true);

  EXPECT_EQ(base::ASCIIToUTF16("Example [US] example.com"), info()->label());
  EXPECT_EQ(base::ASCIIToUTF16(kExampleUrlSecure), info()->Tooltip());
  EXPECT_EQ(url(), info()->displayed_url());
  EXPECT_EQ(ToolbarModel::EV_SECURE, info()->security_level());
}

TEST_F(OriginChipInfoTest, ChromeOrigin) {
  const char kChromeOrigin1[] = "chrome://version/";
  SetURL(kChromeOrigin1, true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ABOUT_VERSION_TITLE),
            info()->label());
  EXPECT_EQ(base::ASCIIToUTF16(kChromeOrigin1), info()->Tooltip());
  EXPECT_EQ(url(), info()->displayed_url());
  EXPECT_EQ(IDR_PRODUCT_LOGO_16, info()->icon());

  // chrome://flags has no title, so the title should be the product name.
  const char kChromeOrigin2[] = "chrome://flags/";
  SetURL(kChromeOrigin2, true);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME), info()->label());
  EXPECT_EQ(base::ASCIIToUTF16(kChromeOrigin2), info()->Tooltip());
  EXPECT_EQ(url(), info()->displayed_url());
  EXPECT_EQ(IDR_PRODUCT_LOGO_16, info()->icon());

  SetURL(kExampleUrl, true);
  EXPECT_NE(IDR_PRODUCT_LOGO_16, info()->icon());
}

TEST_F(OriginChipInfoTest, ExtensionOrigin) {
  CommandLine command_line(CommandLine::NO_PROGRAM);
  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile()));

  // |extension_service| is owned by |profile()|.
  ExtensionService* extension_service =
      test_extension_system->CreateExtensionService(&command_line,
                                                    base::FilePath(),
                                                    false);

  // Create a dummy extension.
  const char kFooId[] = "hhgbjpmdppecanaaogonaigmmifgpaph";
  const char kFooName[] = "Foo";
  extensions::ExtensionBuilder foo_extension;
  foo_extension.SetManifest(extensions::DictionaryBuilder()
                                .Set("name", kFooName)
                                .Set("version", "1.0.0")
                                .Set("manifest_version", 2));
  foo_extension.SetID(kFooId);
  extension_service->AddExtension(foo_extension.Build().get());

  const extensions::IconImage* null_image = NULL;

  // Navigate to a URL from that extension.
  const std::string extension_origin =
      base::StringPrintf("chrome-extension://%s/index.html", kFooId);
  SetURL(extension_origin, true);
  EXPECT_NE(null_image, icon_image());
  EXPECT_EQ(base::ASCIIToUTF16(kFooName), info()->label());
  EXPECT_EQ(base::ASCIIToUTF16(extension_origin), info()->Tooltip());

  SetURL(kExampleUrl, true);
  EXPECT_EQ(null_image, icon_image());
}
