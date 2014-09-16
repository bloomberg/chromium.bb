// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/feature_switch.h"
#include "extensions/common/manifest_constants.h"
#include "extensions/common/manifest_handlers/options_page_info.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace extensions;

namespace errors = extensions::manifest_errors;

class OptionsPageManifestTest : public ChromeManifestTest {
};

TEST_F(OptionsPageManifestTest, OptionsPageInApps) {
  // Allow options page with absolute URL in hosted apps.
  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("hosted_app_absolute_options.json");
  EXPECT_EQ("http://example.com/options.html",
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  extension = LoadAndExpectSuccess("platform_app_with_options_page.json");
  EXPECT_TRUE(!OptionsPageInfo::HasOptionsPage(extension.get()));

  Testcase testcases[] = {
    // Forbid options page with relative URL in hosted apps.
    Testcase("hosted_app_relative_options.json",
             errors::kInvalidOptionsPageInHostedApp),

    // Forbid options page with non-(http|https) scheme in hosted app.
    Testcase("hosted_app_file_options.json",
             errors::kInvalidOptionsPageInHostedApp),

    // Forbid absolute URL for options page in packaged apps.
    Testcase("packaged_app_absolute_options.json",
             errors::kInvalidOptionsPageExpectUrlInPackage)
  };
  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);
}

// Tests for the options_ui.page manifest field.
TEST_F(OptionsPageManifestTest, OptionsUIPage) {
  FeatureSwitch::ScopedOverride enable_flag(
      FeatureSwitch::embedded_extension_options(), true);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("options_ui_page_basic.json");
  EXPECT_EQ(base::StringPrintf("chrome-extension://%s/options.html",
                               extension.get()->id().c_str()),
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  extension = LoadAndExpectSuccess("options_ui_page_with_legacy_page.json");
  EXPECT_EQ(base::StringPrintf("chrome-extension://%s/newoptions.html",
                               extension.get()->id().c_str()),
            OptionsPageInfo::GetOptionsPage(extension.get()).spec());

  Testcase testcases[] = {Testcase("options_ui_page_bad_url.json",
                                   "'page': expected page, got null")};
  RunTestcases(testcases, arraysize(testcases), EXPECT_TYPE_WARNING);
}

// Tests for the options_ui.chrome_style and options_ui.open_in_tab fields.
TEST_F(OptionsPageManifestTest, OptionsUIChromeStyleAndOpenInTab) {
  FeatureSwitch::ScopedOverride enable_flag(
      FeatureSwitch::embedded_extension_options(), true);

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("options_ui_flags_true.json");
  EXPECT_TRUE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

  extension = LoadAndExpectSuccess("options_ui_flags_false.json");
  EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

  // If chrome_style and open_in_tab are not set, they should be false.
  extension = LoadAndExpectSuccess("options_ui_page_basic.json");
  EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_FALSE(OptionsPageInfo::ShouldOpenInTab(extension.get()));
}

// Tests for the options_ui.chrome_style and options_ui.open_in_tab fields when
// the flag for embedded extension options is turned off. chrome_style should
// always be false, open_in_tab should always be true.
TEST_F(OptionsPageManifestTest, OptionsUIChromeStyleAndOpenInTabNoFlag) {
  ASSERT_FALSE(FeatureSwitch::embedded_extension_options()->IsEnabled());

  scoped_refptr<Extension> extension =
      LoadAndExpectSuccess("options_ui_flags_true.json");
  EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

  extension = LoadAndExpectSuccess("options_ui_flags_false.json");
  EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));

  extension = LoadAndExpectSuccess("options_ui_page_basic.json");
  EXPECT_FALSE(OptionsPageInfo::ShouldUseChromeStyle(extension.get()));
  EXPECT_TRUE(OptionsPageInfo::ShouldOpenInTab(extension.get()));
}
