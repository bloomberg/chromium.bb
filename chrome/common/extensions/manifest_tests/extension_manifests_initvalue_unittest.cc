// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/rtl.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/extension_action/page_action_handler.h"
#include "chrome/common/extensions/api/i18n/default_locale_handler.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/manifest_handler.h"
#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"
#include "chrome/common/extensions/manifest_url_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

namespace extensions {

class InitValueManifestTest : public ExtensionManifestTest {
 protected:
  virtual void SetUp() OVERRIDE {
    ExtensionManifestTest::SetUp();
    (new extensions::DefaultLocaleHandler)->Register();
    (new extensions::IconsHandler)->Register();
    (new extensions::OptionsPageHandler)->Register();
    (new extensions::PageActionHandler)->Register();
  }
};

TEST_F(InitValueManifestTest, InitFromValueInvalid) {
  Testcase testcases[] = {
    Testcase("init_invalid_version_missing.json", errors::kInvalidVersion),
    Testcase("init_invalid_version_invalid.json", errors::kInvalidVersion),
    Testcase("init_invalid_name_missing.json", errors::kInvalidName),
    Testcase("init_invalid_name_invalid.json", errors::kInvalidName),
    Testcase("init_invalid_description_invalid.json",
             errors::kInvalidDescription),
    Testcase("init_invalid_icons_invalid.json", errors::kInvalidIcons),
    Testcase("init_invalid_icons_path_invalid.json", errors::kInvalidIconPath),
    Testcase("init_invalid_script_invalid.json",
             errors::kInvalidContentScriptsList),
    Testcase("init_invalid_script_item_invalid.json",
             errors::kInvalidContentScript),
    Testcase("init_invalid_script_matches_missing.json",
             errors::kInvalidMatches),
    Testcase("init_invalid_script_matches_invalid.json",
             errors::kInvalidMatches),
    Testcase("init_invalid_script_matches_empty.json",
             errors::kInvalidMatchCount),
    Testcase("init_invalid_script_match_item_invalid.json",
             errors::kInvalidMatch),
    Testcase("init_invalid_script_match_item_invalid_2.json",
             errors::kInvalidMatch),
    Testcase("init_invalid_script_files_missing.json", errors::kMissingFile),
    Testcase("init_invalid_files_js_invalid.json", errors::kInvalidJsList),
    Testcase("init_invalid_files_empty.json", errors::kMissingFile),
    Testcase("init_invalid_files_js_empty_css_missing.json",
             errors::kMissingFile),
    Testcase("init_invalid_files_js_item_invalid.json", errors::kInvalidJs),
    Testcase("init_invalid_files_css_invalid.json", errors::kInvalidCssList),
    Testcase("init_invalid_files_css_item_invalid.json", errors::kInvalidCss),
    Testcase("init_invalid_permissions_invalid.json",
             errors::kInvalidPermissions),
    Testcase("init_invalid_permissions_item_invalid.json",
             errors::kInvalidPermission),
    Testcase("init_invalid_page_actions_multi.json",
             errors::kInvalidPageActionsListSize),
    Testcase("init_invalid_options_url_invalid.json",
             errors::kInvalidOptionsPage),
    Testcase("init_invalid_locale_invalid.json", errors::kInvalidDefaultLocale),
    Testcase("init_invalid_locale_empty.json", errors::kInvalidDefaultLocale),
    Testcase("init_invalid_min_chrome_invalid.json",
             errors::kInvalidMinimumChromeVersion),
    Testcase("init_invalid_chrome_version_too_low.json",
             errors::kChromeVersionTooLow),
  };

  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_ERROR);
}

TEST_F(InitValueManifestTest, InitFromValueValid) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "init_valid_minimal.json"));

  base::FilePath path;
  PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.AppendASCII("extensions");

  EXPECT_TRUE(Extension::IdIsValid(extension->id()));
  EXPECT_EQ("1.0.0.0", extension->VersionString());
  EXPECT_EQ("my extension", extension->name());
  EXPECT_EQ(extension->id(), extension->url().host());
  EXPECT_EQ(extension->path(), path);
  EXPECT_EQ(path, extension->path());

  // Test permissions scheme.
  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  LoadAndExpectSuccess("init_valid_permissions.json");

  // Test with an options page.
  extension = LoadAndExpectSuccess("init_valid_options.json");
  EXPECT_EQ("chrome-extension",
            ManifestURL::GetOptionsPage(extension).scheme());
  EXPECT_EQ("/options.html",
            ManifestURL::GetOptionsPage(extension).path());

  Testcase testcases[] = {
    // Test that an empty list of page actions does not stop a browser action
    // from being loaded.
    Testcase("init_valid_empty_page_actions.json"),

    // Test with a minimum_chrome_version.
    Testcase("init_valid_minimum_chrome.json"),

    // Test a hosted app with a minimum_chrome_version.
    Testcase("init_valid_app_minimum_chrome.json"),

    // Test a hosted app with a requirements section.
    Testcase("init_valid_app_requirements.json"),

    // Verify empty permission settings are considered valid.
    Testcase("init_valid_permissions_empty.json"),

    // We allow unknown API permissions, so this will be valid until we better
    // distinguish between API and host permissions.
    Testcase("init_valid_permissions_unknown.json")
  };

  RunTestcases(testcases, arraysize(testcases),
               EXPECT_TYPE_SUCCESS);
}

TEST_F(InitValueManifestTest, InitFromValueValidNameInRTL) {
#if defined(TOOLKIT_GTK)
  GtkTextDirection gtk_dir = gtk_widget_get_default_direction();
  gtk_widget_set_default_direction(GTK_TEXT_DIR_RTL);
#else
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");
#endif

  // No strong RTL characters in name.
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "init_valid_name_no_rtl.json"));

  string16 localized_name(ASCIIToUTF16("Dictionary (by Google)"));
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, UTF8ToUTF16(extension->name()));

  // Strong RTL characters in name.
  extension = LoadAndExpectSuccess("init_valid_name_strong_rtl.json");

  localized_name = WideToUTF16(L"Dictionary (\x05D1\x05D2"L" Google)");
  base::i18n::AdjustStringForLocaleDirection(&localized_name);
  EXPECT_EQ(localized_name, UTF8ToUTF16(extension->name()));

  // Reset locale.
#if defined(TOOLKIT_GTK)
  gtk_widget_set_default_direction(gtk_dir);
#else
  base::i18n::SetICUDefaultLocale(locale);
#endif
}

}  // namespace extensions
