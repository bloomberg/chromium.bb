// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#if defined(TOOLKIT_GTK)
#include <gtk/gtk.h>
#endif

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/i18n/rtl.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_sidebar_defaults.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/extensions/url_pattern.h"
#include "content/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"


namespace {

static void AddPattern(URLPatternSet* extent, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  extent->AddPattern(URLPattern(schemes, pattern));
}

}

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

class ExtensionManifestTest : public testing::Test {
 public:
  ExtensionManifestTest() : enable_apps_(true) {}

 protected:
  DictionaryValue* LoadManifestFile(const std::string& filename,
                                    std::string* error) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions")
        .AppendASCII("manifest_tests")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path));

    JSONFileValueSerializer serializer(path);
    return static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));
  }

  scoped_refptr<Extension> LoadExtensionWithLocation(
      DictionaryValue* value,
      Extension::Location location,
      bool strict_error_checks,
      std::string* error) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("manifest_tests");
    int flags = Extension::NO_FLAGS;
    if (strict_error_checks)
      flags |= Extension::STRICT_ERROR_CHECKS;
    return Extension::Create(path.DirName(), location, *value, flags, error);
  }

  scoped_refptr<Extension> LoadExtension(const std::string& name,
                                         std::string* error) {
    return LoadExtensionWithLocation(name, Extension::INTERNAL, false, error);
  }

  scoped_refptr<Extension> LoadExtensionStrict(const std::string& name,
                                               std::string* error) {
    return LoadExtensionWithLocation(name, Extension::INTERNAL, true, error);
  }

  scoped_refptr<Extension> LoadExtension(DictionaryValue* value,
                                         std::string* error) {
    // Loading as an installed extension disables strict error checks.
    return LoadExtensionWithLocation(value, Extension::INTERNAL, false, error);
  }

  scoped_refptr<Extension> LoadExtensionWithLocation(
      const std::string& name,
      Extension::Location location,
      bool strict_error_checks,
      std::string* error) {
    scoped_ptr<DictionaryValue> value(LoadManifestFile(name, error));
    if (!value.get())
      return NULL;
    return LoadExtensionWithLocation(value.get(), location,
                                     strict_error_checks, error);
  }

  scoped_refptr<Extension> LoadAndExpectSuccess(const std::string& name) {
    std::string error;
    scoped_refptr<Extension> extension = LoadExtension(name, &error);
    EXPECT_TRUE(extension) << name;
    EXPECT_EQ("", error) << name;
    return extension;
  }

  scoped_refptr<Extension> LoadStrictAndExpectSuccess(const std::string& name) {
    std::string error;
    scoped_refptr<Extension> extension = LoadExtensionStrict(name, &error);
    EXPECT_TRUE(extension) << name;
    EXPECT_EQ("", error) << name;
    return extension;
  }

  scoped_refptr<Extension> LoadAndExpectSuccess(DictionaryValue* manifest,
                                                const std::string& name) {
    std::string error;
    scoped_refptr<Extension> extension = LoadExtension(manifest, &error);
    EXPECT_TRUE(extension) << "Unexpected failure for " << name;
    EXPECT_EQ("", error) << "Unexpected error for " << name;
    return extension;
  }

  void VerifyExpectedError(Extension* extension,
                           const std::string& name,
                           const std::string& error,
                           const std::string& expected_error) {
    EXPECT_FALSE(extension) <<
        "Expected failure loading extension '" << name <<
        "', but didn't get one.";
    EXPECT_TRUE(MatchPattern(error, expected_error)) << name <<
        " expected '" << expected_error << "' but got '" << error << "'";
  }

  void LoadAndExpectError(const std::string& name,
                          const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension(LoadExtension(name, &error));
    VerifyExpectedError(extension.get(), name, error, expected_error);
  }

  void LoadAndExpectErrorStrict(const std::string& name,
                                const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension(LoadExtensionStrict(name, &error));
    VerifyExpectedError(extension.get(), name, error, expected_error);
  }

  void LoadAndExpectError(DictionaryValue* manifest,
                          const std::string& name,
                          const std::string& expected_error) {
    std::string error;
    scoped_refptr<Extension> extension(LoadExtension(manifest, &error));
    VerifyExpectedError(extension.get(), name, error, expected_error);
  }

  struct Testcase {
    std::string manifest;
    std::string expected_error;
  };

  void RunTestcases(const Testcase* testcases, size_t num_testcases) {
    for (size_t i = 0; i < num_testcases; ++i) {
      LoadAndExpectError(testcases[i].manifest, testcases[i].expected_error);
    }
  }

  bool enable_apps_;
};

TEST_F(ExtensionManifestTest, InitFromValueInvalid) {
  Testcase testcases[] = {
    {"init_invalid_version_missing.json", errors::kInvalidVersion},
    {"init_invalid_version_invalid.json", errors::kInvalidVersion},
    {"init_invalid_name_missing.json", errors::kInvalidName},
    {"init_invalid_name_invalid.json", errors::kInvalidName},
    {"init_invalid_description_invalid.json", errors::kInvalidDescription},
    {"init_invalid_icons_invalid.json", errors::kInvalidIcons},
    {"init_invalid_icons_path_invalid.json", errors::kInvalidIconPath},
    {"init_invalid_script_invalid.json", errors::kInvalidContentScriptsList},
    {"init_invalid_script_item_invalid.json", errors::kInvalidContentScript},
    {"init_invalid_script_matches_missing.json", errors::kInvalidMatches},
    {"init_invalid_script_matches_invalid.json", errors::kInvalidMatches},
    {"init_invalid_script_matches_empty.json", errors::kInvalidMatchCount},
    {"init_invalid_script_match_item_invalid.json", errors::kInvalidMatch},
    {"init_invalid_script_match_item_invalid_2.json", errors::kInvalidMatch},
    {"init_invalid_script_files_missing.json", errors::kMissingFile},
    {"init_invalid_files_js_invalid.json", errors::kInvalidJsList},
    {"init_invalid_files_empty.json", errors::kMissingFile},
    {"init_invalid_files_js_empty_css_missing.json", errors::kMissingFile},
    {"init_invalid_files_js_item_invalid.json", errors::kInvalidJs},
    {"init_invalid_files_css_invalid.json", errors::kInvalidCssList},
    {"init_invalid_files_css_item_invalid.json", errors::kInvalidCss},
    {"init_invalid_permissions_invalid.json", errors::kInvalidPermissions},
    {"init_invalid_permissions_item_invalid.json", errors::kInvalidPermission},
    {"init_invalid_page_actions_multi.json",
        errors::kInvalidPageActionsListSize},
    {"init_invalid_options_url_invalid.json", errors::kInvalidOptionsPage},
    {"init_invalid_locale_invalid.json", errors::kInvalidDefaultLocale},
    {"init_invalid_locale_empty.json", errors::kInvalidDefaultLocale},
    {"init_invalid_min_chrome_invalid.json",
        errors::kInvalidMinimumChromeVersion},
    {"init_invalid_chrome_version_too_low.json", errors::kChromeVersionTooLow}
  };

  RunTestcases(testcases, arraysize(testcases));
}

TEST_F(ExtensionManifestTest, InitFromValueValid) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess(
      "init_valid_minimal.json"));

  FilePath path;
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
  extension = LoadAndExpectSuccess("init_valid_permissions.json");

  // Test with an options page.
  extension = LoadAndExpectSuccess("init_valid_options.json");
  EXPECT_EQ("chrome-extension", extension->options_url().scheme());
  EXPECT_EQ("/options.html", extension->options_url().path());

  // Test that an empty list of page actions does not stop a browser action
  // from being loaded.
  extension = LoadAndExpectSuccess("init_valid_empty_page_actions.json");

  // Test with a minimum_chrome_version.
  extension = LoadAndExpectSuccess("init_valid_minimum_chrome.json");

  // Test a hosted app with a minimum_chrome_version.
  extension = LoadAndExpectSuccess("init_valid_app_minimum_chrome.json");

  // Verify empty permission settings are considered valid.
  LoadAndExpectSuccess("init_valid_permissions_empty.json");

  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  LoadAndExpectSuccess("init_valid_permissions_unknown.json");
}

TEST_F(ExtensionManifestTest, InitFromValueValidNameInRTL) {
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

TEST_F(ExtensionManifestTest, UpdateUrls) {
  // Test several valid update urls
  LoadStrictAndExpectSuccess("update_url_valid_1.json");
  LoadStrictAndExpectSuccess("update_url_valid_2.json");
  LoadStrictAndExpectSuccess("update_url_valid_3.json");
  LoadStrictAndExpectSuccess("update_url_valid_4.json");

  // Test some invalid update urls
  LoadAndExpectErrorStrict("update_url_invalid_1.json",
      errors::kInvalidUpdateURL);
  LoadAndExpectErrorStrict("update_url_invalid_2.json",
      errors::kInvalidUpdateURL);
  LoadAndExpectErrorStrict("update_url_invalid_3.json",
      errors::kInvalidUpdateURL);
}

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ExtensionManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<Extension> extension = LoadStrictAndExpectSuccess(
      "old_unlimited_storage.json");
  EXPECT_TRUE(extension->HasAPIPermission(
      ExtensionAPIPermission::kUnlimitedStorage));
}

TEST_F(ExtensionManifestTest, ValidApp) {
  scoped_refptr<Extension> extension(LoadAndExpectSuccess("valid_app.json"));
  URLPatternSet expected_patterns;
  AddPattern(&expected_patterns, "http://www.google.com/mail/*");
  AddPattern(&expected_patterns, "http://www.google.com/foobar/*");
  EXPECT_EQ(expected_patterns, extension->web_extent());
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());
  EXPECT_EQ("http://www.google.com/mail/", extension->launch_web_url());
}

TEST_F(ExtensionManifestTest, AppWebUrls) {
  LoadAndExpectError("web_urls_wrong_type.json",
                     errors::kInvalidWebURLs);
  LoadAndExpectError(
      "web_urls_invalid_1.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(0),
          errors::kExpectString));

  LoadAndExpectError(
      "web_urls_invalid_2.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(0),
          URLPattern::GetParseResultString(
              URLPattern::PARSE_ERROR_MISSING_SCHEME_SEPARATOR)));

  LoadAndExpectError(
      "web_urls_invalid_3.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(0),
          errors::kNoWildCardsInPaths));

  LoadAndExpectError(
      "web_urls_invalid_4.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(0),
          errors::kCannotClaimAllURLsInExtent));

  LoadAndExpectError(
      "web_urls_invalid_5.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(1),
          errors::kCannotClaimAllHostsInExtent));

  // Ports in app.urls only raise an error when loading as a
  // developer would.
  LoadAndExpectSuccess("web_urls_invalid_has_port.json");
  LoadAndExpectErrorStrict(
      "web_urls_invalid_has_port.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(1),
          URLPattern::GetParseResultString(URLPattern::PARSE_ERROR_HAS_COLON)));


  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}

TEST_F(ExtensionManifestTest, AppLaunchContainer) {
  scoped_refptr<Extension> extension;

  extension = LoadAndExpectSuccess("launch_tab.json");
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());

  extension = LoadAndExpectSuccess("launch_panel.json");
  EXPECT_EQ(extension_misc::LAUNCH_PANEL, extension->launch_container());

  extension = LoadAndExpectSuccess("launch_default.json");
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());

  extension = LoadAndExpectSuccess("launch_width.json");
  EXPECT_EQ(640, extension->launch_width());

  extension = LoadAndExpectSuccess("launch_height.json");
  EXPECT_EQ(480, extension->launch_height());

  LoadAndExpectError("launch_window.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_invalid_type.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_invalid_value.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_without_launch_url.json",
                     errors::kLaunchURLRequired);
  LoadAndExpectError("launch_width_invalid.json",
                     errors::kInvalidLaunchWidthContainer);
  LoadAndExpectError("launch_width_negative.json",
                     errors::kInvalidLaunchWidth);
  LoadAndExpectError("launch_height_invalid.json",
                     errors::kInvalidLaunchHeightContainer);
  LoadAndExpectError("launch_height_negative.json",
                     errors::kInvalidLaunchHeight);
}

TEST_F(ExtensionManifestTest, AppLaunchURL) {
  LoadAndExpectError("launch_path_and_url.json",
                     errors::kLaunchPathAndURLAreExclusive);
  LoadAndExpectError("launch_path_and_extent.json",
                     errors::kLaunchPathAndExtentAreExclusive);
  LoadAndExpectError("launch_path_invalid_type.json",
                     errors::kInvalidLaunchLocalPath);
  LoadAndExpectError("launch_path_invalid_value.json",
                     errors::kInvalidLaunchLocalPath);
  LoadAndExpectError("launch_url_invalid_type_1.json",
                     errors::kInvalidLaunchWebURL);
  LoadAndExpectError("launch_url_invalid_type_2.json",
                     errors::kInvalidLaunchWebURL);
  LoadAndExpectError("launch_url_invalid_type_3.json",
                     errors::kInvalidLaunchWebURL);

  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("launch_local_path.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  LoadAndExpectError("launch_web_url_relative.json",
                     errors::kInvalidLaunchWebURL);

  extension = LoadAndExpectSuccess("launch_web_url_absolute.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
}

TEST_F(ExtensionManifestTest, Override) {
  LoadAndExpectError("override_newtab_and_history.json",
                     errors::kMultipleOverrides);
  LoadAndExpectError("override_invalid_page.json",
                     errors::kInvalidChromeURLOverrides);

  scoped_refptr<Extension> extension;

  extension = LoadAndExpectSuccess("override_new_tab.json");
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extension->GetChromeURLOverrides().find("newtab")->second.spec());

  extension = LoadAndExpectSuccess("override_history.json");
  EXPECT_EQ(extension->url().spec() + "history.html",
            extension->GetChromeURLOverrides().find("history")->second.spec());
}

TEST_F(ExtensionManifestTest, ChromeURLPermissionInvalid) {
  LoadAndExpectError("permission_chrome_url_invalid.json",
      errors::kInvalidPermissionScheme);
}

TEST_F(ExtensionManifestTest, ChromeResourcesPermissionValidOnlyForComponents) {
  LoadAndExpectError("permission_chrome_resources_url.json",
      errors::kInvalidPermissionScheme);
  std::string error;
  scoped_refptr<Extension> extension;
  extension = LoadExtensionWithLocation(
      "permission_chrome_resources_url.json",
      Extension::COMPONENT,
      true,  // Strict error checking
      &error);
  EXPECT_EQ("", error);
}

TEST_F(ExtensionManifestTest, InvalidContentScriptMatchPattern) {

  // chrome:// urls are not allowed.
  LoadAndExpectError(
      "content_script_chrome_url_invalid.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidMatch,
          base::IntToString(0),
          base::IntToString(0),
          URLPattern::GetParseResultString(
              URLPattern::PARSE_ERROR_INVALID_SCHEME)));

  // Match paterns must be strings.
  LoadAndExpectError(
      "content_script_match_pattern_not_string.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidMatch,
          base::IntToString(0),
          base::IntToString(0),
          errors::kExpectString));

  // Ports in match patterns cause an error, but only when loading
  // in developer mode.
  LoadAndExpectSuccess("forbid_ports_in_content_scripts.json");

  // Loading as a developer would should give an error.
  LoadAndExpectErrorStrict(
      "forbid_ports_in_content_scripts.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidMatch,
          base::IntToString(1),
          base::IntToString(0),
          URLPattern::GetParseResultString(
              URLPattern::PARSE_ERROR_HAS_COLON)));
}

TEST_F(ExtensionManifestTest, ExcludeMatchPatterns) {
  LoadAndExpectSuccess("exclude_matches.json");
  LoadAndExpectSuccess("exclude_matches_empty.json");

  LoadAndExpectError("exclude_matches_not_list.json",
                     "Invalid value for 'content_scripts[0].exclude_matches'.");

  LoadAndExpectError("exclude_matches_invalid_host.json",
                     "Invalid value for "
                     "'content_scripts[0].exclude_matches[0]': "
                     "Invalid host wildcard.");
}

TEST_F(ExtensionManifestTest, ExperimentalPermission) {
  LoadAndExpectError("experimental.json", errors::kExperimentalFlagRequired);
  CommandLine old_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  LoadAndExpectSuccess("experimental.json");
  *CommandLine::ForCurrentProcess() = old_command_line;
}

TEST_F(ExtensionManifestTest, DevToolsExtensions) {
  LoadAndExpectError("devtools_extension_no_permissions.json",
      errors::kDevToolsExperimental);
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
      errors::kInvalidDevToolsPage);

  CommandLine old_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("devtools_extension.json");
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extension->devtools_url().spec());
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());

  *CommandLine::ForCurrentProcess() = old_command_line;
}

TEST_F(ExtensionManifestTest, Sidebar) {
  LoadAndExpectError("sidebar.json",
      errors::kExperimentalFlagRequired);

  CommandLine old_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  LoadAndExpectError("sidebar_no_permissions.json",
      errors::kSidebarExperimental);

  LoadAndExpectError("sidebar_icon_empty.json",
      errors::kInvalidSidebarDefaultIconPath);
  LoadAndExpectError("sidebar_icon_invalid_type.json",
      errors::kInvalidSidebarDefaultIconPath);
  LoadAndExpectError("sidebar_page_empty.json",
      errors::kInvalidSidebarDefaultPage);
  LoadAndExpectError("sidebar_page_invalid_type.json",
      errors::kInvalidSidebarDefaultPage);
  LoadAndExpectError("sidebar_title_invalid_type.json",
      errors::kInvalidSidebarDefaultTitle);

  scoped_refptr<Extension> extension(LoadAndExpectSuccess("sidebar.json"));
  ASSERT_TRUE(extension->sidebar_defaults() != NULL);
  EXPECT_EQ(extension->sidebar_defaults()->default_title(),
            ASCIIToUTF16("Default title"));
  EXPECT_EQ(extension->sidebar_defaults()->default_icon_path(),
            "icon.png");
  EXPECT_EQ(extension->url().spec() + "sidebar.html",
            extension->sidebar_defaults()->default_page().spec());

  *CommandLine::ForCurrentProcess() = old_command_line;
}

TEST_F(ExtensionManifestTest, DisallowHybridApps) {
  LoadAndExpectError("disallow_hybrid_1.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kHostedAppsCannotIncludeExtensionFeatures,
          keys::kBrowserAction));
  LoadAndExpectError("disallow_hybrid_2.json",
                     errors::kBackgroundPermissionNeeded);
}

TEST_F(ExtensionManifestTest, OptionsPageInApps) {
  scoped_refptr<Extension> extension;

  // Allow options page with absolute URL in hosted apps.
  extension = LoadAndExpectSuccess("hosted_app_absolute_options.json");
  EXPECT_STREQ("http",
               extension->options_url().scheme().c_str());
  EXPECT_STREQ("example.com",
               extension->options_url().host().c_str());
  EXPECT_STREQ("options.html",
               extension->options_url().ExtractFileName().c_str());

  // Forbid options page with relative URL in hosted apps.
  LoadAndExpectError("hosted_app_relative_options.json",
                     errors::kInvalidOptionsPageInHostedApp);

  // Forbid options page with non-(http|https) scheme in hosted app.
  LoadAndExpectError("hosted_app_file_options.json",
                     errors::kInvalidOptionsPageInHostedApp);

  // Forbid absolute URL for options page in packaged apps.
  LoadAndExpectError("packaged_app_absolute_options.json",
                     errors::kInvalidOptionsPageExpectUrlInPackage);
}

TEST_F(ExtensionManifestTest, AllowUnrecognizedPermissions) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("valid_app.json", &error));
  ASSERT_TRUE(manifest.get());
  CommandLine old_command_line = *CommandLine::ForCurrentProcess();

  ListValue *permissions = new ListValue();
  manifest->Set(keys::kPermissions, permissions);
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet api_perms = info->GetAll();
  for (ExtensionAPIPermissionSet::iterator i = api_perms.begin();
       i != api_perms.end(); ++i) {
    ExtensionAPIPermission* permission = info->GetByID(*i);
    const char* name = permission->name();
    StringValue* p = new StringValue(name);
    permissions->Clear();
    permissions->Append(p);
    std::string message_name = base::StringPrintf("permission-%s", name);

    if (*i == ExtensionAPIPermission::kExperimental) {
      // Experimental permission is allowed, but requires this switch.
      CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableExperimentalExtensionApis);
    }

    // Extensions are allowed to contain unrecognized API permissions,
    // so there shouldn't be any errors.
    scoped_refptr<Extension> extension;
    extension = LoadAndExpectSuccess(manifest.get(), message_name);
  }
  *CommandLine::ForCurrentProcess() = old_command_line;
}

TEST_F(ExtensionManifestTest, NormalizeIconPaths) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("normalize_icon_paths.json"));
  EXPECT_EQ("16.png",
            extension->icons().Get(16, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png",
            extension->icons().Get(48, ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, DisallowMultipleUISurfaces) {
  LoadAndExpectError("multiple_ui_surfaces_1.json", errors::kOneUISurfaceOnly);
  LoadAndExpectError("multiple_ui_surfaces_2.json", errors::kOneUISurfaceOnly);
  LoadAndExpectError("multiple_ui_surfaces_3.json", errors::kOneUISurfaceOnly);
}

TEST_F(ExtensionManifestTest, ParseHomepageURLs) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));
  LoadAndExpectError("homepage_empty.json",
                     extension_manifest_errors::kInvalidHomepageURL);
  LoadAndExpectError("homepage_invalid.json",
                     extension_manifest_errors::kInvalidHomepageURL);
  LoadAndExpectError("homepage_bad_schema.json",
                     extension_manifest_errors::kInvalidHomepageURL);
}

TEST_F(ExtensionManifestTest, GetHomepageURL) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("homepage_valid.json"));
  EXPECT_EQ(GURL("http://foo.com#bar"), extension->GetHomepageURL());

  // The Google Gallery URL ends with the id, which depends on the path, which
  // can be different in testing, so we just check the part before id.
  extension = LoadAndExpectSuccess("homepage_google_hosted.json");
  EXPECT_TRUE(StartsWithASCII(extension->GetHomepageURL().spec(),
                              "https://chrome.google.com/webstore/detail/",
                              false));

  extension = LoadAndExpectSuccess("homepage_externally_hosted.json");
  EXPECT_EQ(GURL(), extension->GetHomepageURL());
}

TEST_F(ExtensionManifestTest, DefaultPathForExtent) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("default_path_for_extent.json"));

  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("/*", extension->web_extent().patterns().begin()->path());
  EXPECT_TRUE(extension->web_extent().MatchesURL(
      GURL("http://www.google.com/monkey")));
}

TEST_F(ExtensionManifestTest, DefaultLocale) {
  LoadAndExpectError("default_locale_invalid.json",
                     extension_manifest_errors::kInvalidDefaultLocale);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("default_locale_valid.json"));
  EXPECT_EQ("de-AT", extension->default_locale());
}

TEST_F(ExtensionManifestTest, TtsEngine) {
  LoadAndExpectError("tts_engine_invalid_1.json",
                     extension_manifest_errors::kInvalidTts);
  LoadAndExpectError("tts_engine_invalid_2.json",
                     extension_manifest_errors::kInvalidTtsVoices);
  LoadAndExpectError("tts_engine_invalid_3.json",
                     extension_manifest_errors::kInvalidTtsVoices);
  LoadAndExpectError("tts_engine_invalid_4.json",
                     extension_manifest_errors::kInvalidTtsVoicesVoiceName);
  LoadAndExpectError("tts_engine_invalid_5.json",
                     extension_manifest_errors::kInvalidTtsVoicesLang);
  LoadAndExpectError("tts_engine_invalid_6.json",
                     extension_manifest_errors::kInvalidTtsVoicesLang);
  LoadAndExpectError("tts_engine_invalid_7.json",
                     extension_manifest_errors::kInvalidTtsVoicesGender);
  LoadAndExpectError("tts_engine_invalid_8.json",
                     extension_manifest_errors::kInvalidTtsVoicesEventTypes);
  LoadAndExpectError("tts_engine_invalid_9.json",
                     extension_manifest_errors::kInvalidTtsVoicesEventTypes);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("tts_engine_valid.json"));

  ASSERT_EQ(1u, extension->tts_voices().size());
  EXPECT_EQ("name", extension->tts_voices()[0].voice_name);
  EXPECT_EQ("en-US", extension->tts_voices()[0].lang);
  EXPECT_EQ("female", extension->tts_voices()[0].gender);
  EXPECT_EQ(3U, extension->tts_voices()[0].event_types.size());
}

TEST_F(ExtensionManifestTest, WebIntents) {
  CommandLine::ForCurrentProcess()->AppendSwitch("--enable-web-intents");

  LoadAndExpectError("intent_invalid_1.json",
                     extension_manifest_errors::kInvalidIntents);
  LoadAndExpectError("intent_invalid_2.json",
                     extension_manifest_errors::kInvalidIntent);
  LoadAndExpectError("intent_invalid_3.json",
                     extension_manifest_errors::kInvalidIntentPath);
  LoadAndExpectError("intent_invalid_4.json",
                     extension_manifest_errors::kInvalidIntentDisposition);
  LoadAndExpectError("intent_invalid_5.json",
                     extension_manifest_errors::kInvalidIntentType);
  LoadAndExpectError("intent_invalid_6.json",
                     extension_manifest_errors::kInvalidIntentTitle);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid.json"));
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents().size());
  EXPECT_EQ("image/png", UTF16ToUTF8(extension->intents()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents()[0].action));
  EXPECT_EQ("chrome-extension", extension->intents()[0].service_url.scheme());
  EXPECT_EQ("///services/share", extension->intents()[0].service_url.path());
  EXPECT_EQ("Sample Sharing Intent",
            UTF16ToUTF8(extension->intents()[0].title));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_INLINE,
            extension->intents()[0].disposition);

  // Verify that optional fields are filled with defaults.
  extension = LoadAndExpectSuccess("intent_valid_minimal.json");
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents().size());
  EXPECT_EQ("", UTF16ToUTF8(extension->intents()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents()[0].action));
  EXPECT_TRUE(extension->intents()[0].service_url.is_empty());
  EXPECT_EQ("", UTF16ToUTF8(extension->intents()[0].title));
  EXPECT_EQ(WebIntentServiceData::DISPOSITION_WINDOW,
            extension->intents()[0].disposition);
}

TEST_F(ExtensionManifestTest, ForbidPortsInPermissions) {
  // Loading as a user would shoud not trigger an error.
  LoadAndExpectSuccess("forbid_ports_in_permissions.json");

  // Ideally, loading as a developer would give an error.
  // To ensure that we do not error out on a valid permission
  // in a future version of chrome, validation is to loose
  // to flag this case.
  LoadStrictAndExpectSuccess("forbid_ports_in_permissions.json");
}

TEST_F(ExtensionManifestTest, IsolatedApps) {
  // Requires --enable-experimental-extension-apis
  LoadAndExpectError("isolated_app_valid.json",
                     errors::kExperimentalFlagRequired);

  CommandLine old_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("isolated_app_valid.json"));
  EXPECT_TRUE(extension2->is_storage_isolated());
  *CommandLine::ForCurrentProcess() = old_command_line;
}


TEST_F(ExtensionManifestTest, FileBrowserHandlers) {
  LoadAndExpectError("filebrowser_invalid_actions_1.json",
      errors::kInvalidFileBrowserHandler);
  LoadAndExpectError("filebrowser_invalid_actions_2.json",
      errors::kInvalidFileBrowserHandler);
  LoadAndExpectError("filebrowser_invalid_action_id.json",
      errors::kInvalidPageActionId);
  LoadAndExpectError("filebrowser_invalid_action_title.json",
      errors::kInvalidPageActionDefaultTitle);
  LoadAndExpectError("filebrowser_invalid_action_id.json",
      errors::kInvalidPageActionId);
  LoadAndExpectError("filebrowser_invalid_file_filters_1.json",
      errors::kInvalidFileFiltersList);
  LoadAndExpectError("filebrowser_invalid_file_filters_2.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidFileFilterValue, base::IntToString(0)));
  LoadAndExpectError("filebrowser_invalid_file_filters_url.json",
      ExtensionErrorUtils::FormatErrorMessage(errors::kInvalidURLPatternError,
                                              "http:*.html"));

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("filebrowser_valid.json"));
  ASSERT_TRUE(extension->file_browser_handlers() != NULL);
  ASSERT_EQ(extension->file_browser_handlers()->size(), 1U);
  const FileBrowserHandler* action =
      extension->file_browser_handlers()->at(0).get();
  EXPECT_EQ(action->title(), "Default title");
  EXPECT_EQ(action->icon_path(), "icon.png");
  const URLPatternSet& patterns = action->file_url_patterns();
  ASSERT_EQ(patterns.patterns().size(), 1U);
  ASSERT_TRUE(action->MatchesURL(
      GURL("filesystem:chrome-extension://foo/local/test.txt")));
}

TEST_F(ExtensionManifestTest, FileManagerURLOverride) {
  // A component extention can override chrome://files/ URL.
  std::string error;
  scoped_refptr<Extension> extension;
  extension = LoadExtensionWithLocation(
      "filebrowser_url_override.json",
      Extension::COMPONENT,
      true,  // Strict error checking
      &error);
#if defined(FILE_MANAGER_EXTENSION)
  EXPECT_EQ("", error);
#else
  EXPECT_EQ(errors::kInvalidChromeURLOverrides, error);
#endif

  // Extensions of other types can't ovverride chrome://files/ URL.
  LoadAndExpectError("filebrowser_url_override.json",
      errors::kInvalidChromeURLOverrides);
}

TEST_F(ExtensionManifestTest, OfflineEnabled) {
  LoadAndExpectError("offline_enabled_invalid.json",
                     errors::kInvalidOfflineEnabled);
  scoped_refptr<Extension> extension_0(
      LoadAndExpectSuccess("offline_enabled_extension.json"));
  EXPECT_TRUE(extension_0->offline_enabled());
  scoped_refptr<Extension> extension_1(
      LoadAndExpectSuccess("offline_enabled_packaged_app.json"));
  EXPECT_TRUE(extension_1->offline_enabled());
  scoped_refptr<Extension> extension_2(
      LoadAndExpectSuccess("offline_disabled_packaged_app.json"));
  EXPECT_FALSE(extension_2->offline_enabled());
  scoped_refptr<Extension> extension_3(
      LoadAndExpectSuccess("offline_default_packaged_app.json"));
  EXPECT_FALSE(extension_3->offline_enabled());
  scoped_refptr<Extension> extension_4(
      LoadAndExpectSuccess("offline_enabled_hosted_app.json"));
  EXPECT_TRUE(extension_4->offline_enabled());
}
