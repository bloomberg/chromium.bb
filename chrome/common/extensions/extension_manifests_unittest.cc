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
#include "base/json/json_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_sidebar_defaults.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/extensions/url_pattern.h"
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
  static DictionaryValue* LoadManifestFile(const std::string& filename,
                                           std::string* error) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions")
        .AppendASCII("manifest_tests")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();

    JSONFileValueSerializer serializer(path);
    return static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));
  }

  // Helper class that simplifies creating methods that take either a filename
  // to a manifest or the manifest itself.
  class Manifest {
   public:
    // Purposely not marked explicit for convenience. The vast majority of
    // callers pass string literal.
    Manifest(const char* name)
        : name_(name), manifest_(NULL) {
    }
    Manifest(DictionaryValue* manifest, const char* name)
        : name_(name), manifest_(manifest) {
    }

    const std::string& name() const { return name_; }

    DictionaryValue* GetManifest(std::string* error) const {
      if (manifest_)
        return manifest_;

      manifest_ = LoadManifestFile(name_, error);
      manifest_holder_.reset(manifest_);
      return manifest_;
    }

   private:
    std::string name_;
    mutable DictionaryValue* manifest_;
    mutable scoped_ptr<DictionaryValue> manifest_holder_;
  };

  scoped_refptr<Extension> LoadExtension(
      const Manifest& manifest,
      std::string* error,
      Extension::Location location = Extension::INTERNAL,
      int flags = Extension::NO_FLAGS) {
    DictionaryValue* value = manifest.GetManifest(error);
    if (!value)
      return NULL;
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("manifest_tests");
    return Extension::Create(path.DirName(), location, *value, flags, error);
  }

  scoped_refptr<Extension> LoadAndExpectSuccess(
      const Manifest& manifest,
      Extension::Location location = Extension::INTERNAL,
      int flags = Extension::NO_FLAGS) {
    std::string error;
    scoped_refptr<Extension> extension =
        LoadExtension(manifest, &error, location, flags);
    EXPECT_TRUE(extension) << manifest.name();
    EXPECT_EQ("", error) << manifest.name();
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

  void LoadAndExpectError(const Manifest& manifest,
                          const std::string& expected_error,
                          Extension::Location location = Extension::INTERNAL,
                          int flags = Extension::NO_FLAGS) {
    std::string error;
    scoped_refptr<Extension> extension(
        LoadExtension(manifest, &error, location, flags));
    VerifyExpectedError(extension.get(), manifest.name(), error,
                        expected_error);
  }

  struct Testcase {
    std::string manifest;
    std::string expected_error;
  };

  void RunTestcases(const Testcase* testcases, size_t num_testcases) {
    for (size_t i = 0; i < num_testcases; ++i) {
      LoadAndExpectError(testcases[i].manifest.c_str(),
                         testcases[i].expected_error);
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
    {"init_invalid_chrome_version_too_low.json", errors::kChromeVersionTooLow},
    {"init_invalid_requirements_1.json", errors::kInvalidRequirements},
    {"init_invalid_requirements_2.json", errors::kInvalidRequirement}
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
  LoadAndExpectSuccess("init_valid_empty_page_actions.json");

  // Test with a minimum_chrome_version.
  LoadAndExpectSuccess("init_valid_minimum_chrome.json");

  // Test a hosted app with a minimum_chrome_version.
  LoadAndExpectSuccess("init_valid_app_minimum_chrome.json");

  // Test a hosted app with a requirements section.
  LoadAndExpectSuccess("init_valid_app_requirements.json");

  // Verify empty permission settings are considered valid.
  LoadAndExpectSuccess("init_valid_permissions_empty.json");

  // We allow unknown API permissions, so this will be valid until we better
  // distinguish between API and host permissions.
  LoadAndExpectSuccess("init_valid_permissions_unknown.json");
}

TEST_F(ExtensionManifestTest, PlatformApps) {
  // A minimal platform app.
  LoadAndExpectSuccess("init_valid_platform_app.json");
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
  LoadAndExpectSuccess("update_url_valid_1.json", Extension::INTERNAL,
                       Extension::STRICT_ERROR_CHECKS);
  LoadAndExpectSuccess("update_url_valid_2.json", Extension::INTERNAL,
                       Extension::STRICT_ERROR_CHECKS);
  LoadAndExpectSuccess("update_url_valid_3.json", Extension::INTERNAL,
                       Extension::STRICT_ERROR_CHECKS);
  LoadAndExpectSuccess("update_url_valid_4.json", Extension::INTERNAL,
                       Extension::STRICT_ERROR_CHECKS);

  // Test some invalid update urls
  LoadAndExpectError("update_url_invalid_1.json", errors::kInvalidUpdateURL,
                     Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);
  LoadAndExpectError("update_url_invalid_2.json", errors::kInvalidUpdateURL,
                     Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);
  LoadAndExpectError("update_url_invalid_3.json", errors::kInvalidUpdateURL,
                     Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);
}

// Tests that the old permission name "unlimited_storage" still works for
// backwards compatibility (we renamed it to "unlimitedStorage").
TEST_F(ExtensionManifestTest, OldUnlimitedStoragePermission) {
  scoped_refptr<Extension> extension = LoadAndExpectSuccess(
      "old_unlimited_storage.json", Extension::INTERNAL,
      Extension::STRICT_ERROR_CHECKS);
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
  LoadAndExpectError(
      "web_urls_invalid_has_port.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidWebURL,
          base::IntToString(1),
          URLPattern::GetParseResultString(URLPattern::PARSE_ERROR_HAS_COLON)),
      Extension::INTERNAL,
      Extension::STRICT_ERROR_CHECKS);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns().begin()->GetAsString());
}

TEST_F(ExtensionManifestTest, AppLaunchContainer) {
  scoped_refptr<Extension> extension;
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePlatformApps);

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
  LoadAndExpectError("launch_container_invalid_type_for_platform.json",
                     errors::kInvalidLaunchContainerForPlatform);
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
  extension = LoadExtension(
      "permission_chrome_resources_url.json",
      &error,
      Extension::COMPONENT,
      Extension::STRICT_ERROR_CHECKS);
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
  LoadAndExpectError(
      "forbid_ports_in_content_scripts.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidMatch,
          base::IntToString(1),
          base::IntToString(0),
          URLPattern::GetParseResultString(
              URLPattern::PARSE_ERROR_HAS_COLON)),
      Extension::INTERNAL,
      Extension::STRICT_ERROR_CHECKS);
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
  LoadAndExpectSuccess("experimental.json", Extension::COMPONENT);
  LoadAndExpectSuccess("experimental.json", Extension::INTERNAL,
                       Extension::FROM_WEBSTORE);
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  LoadAndExpectSuccess("experimental.json");
}

TEST_F(ExtensionManifestTest, DevToolsExtensions) {
  LoadAndExpectError("devtools_extension_no_permissions.json",
      errors::kDevToolsExperimental);
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
      errors::kInvalidDevToolsPage);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("devtools_extension.json");
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extension->devtools_url().spec());
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());
}

TEST_F(ExtensionManifestTest, Sidebar) {
  LoadAndExpectError("sidebar.json",
      errors::kExperimentalFlagRequired);

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

TEST_F(ExtensionManifestTest, HostedAppPermissions) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("hosted_app_absolute_options.json", &error));
  ASSERT_TRUE(manifest.get());
  ListValue* permissions = NULL;
  ASSERT_TRUE(manifest->GetList("permissions", &permissions));

  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet api_perms = info->GetAll();
  for (ExtensionAPIPermissionSet::iterator i = api_perms.begin();
       i != api_perms.end(); ++i) {
    if (*i == ExtensionAPIPermission::kExperimental)
      continue;

    ExtensionAPIPermission* permission = info->GetByID(*i);
    const char* name = permission->name();
    StringValue* p = new StringValue(name);
    permissions->Clear();
    permissions->Append(p);

    // Some permissions are only available to component hosted apps.
    if (permission->is_component_only()) {
      LoadAndExpectError(Manifest(manifest.get(), name),
                         errors::kPermissionNotAllowed,
                         Extension::INTERNAL);
      scoped_refptr<Extension> extension(
          LoadAndExpectSuccess(Manifest(manifest.get(), name),
                               Extension::COMPONENT));
      EXPECT_TRUE(extension->GetActivePermissions()->HasAPIPermission(
          permission->id()));

    } else if (permission->is_platform_app_only()) {
      LoadAndExpectError(Manifest(manifest.get(), name),
                         errors::kPermissionNotAllowed,
                         Extension::INTERNAL,
                         Extension::STRICT_ERROR_CHECKS);
    } else if (!permission->is_hosted_app()) {
      // Most normal extension permissions also aren't available to hosted apps.
      // For these, the error is only reported in strict mode for legacy
      // reasons: crbug.com/101993.
      LoadAndExpectError(Manifest(manifest.get(), name),
                         errors::kPermissionNotAllowed,
                         Extension::INTERNAL,
                         Extension::STRICT_ERROR_CHECKS);
      scoped_refptr<Extension> extension(
          LoadAndExpectSuccess(Manifest(manifest.get(), name),
                               Extension::INTERNAL));
      EXPECT_FALSE(extension->GetActivePermissions()->HasAPIPermission(
          permission->id()));

      // These permissions are also allowed for component hosted apps.
      extension = LoadAndExpectSuccess(Manifest(manifest.get(), name),
                                       Extension::COMPONENT);
      EXPECT_TRUE(extension->GetActivePermissions()->HasAPIPermission(
          permission->id()));

    } else {
      scoped_refptr<Extension> extension(
          LoadAndExpectSuccess(Manifest(manifest.get(), name)));
      EXPECT_TRUE(extension->GetActivePermissions()->HasAPIPermission(
          permission->id()));
    }
  }
}

TEST_F(ExtensionManifestTest, ComponentOnlyPermission) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("init_valid_minimal.json", &error));
  ASSERT_TRUE(manifest.get());
  ListValue* permissions = new ListValue();
  manifest->Set("permissions", permissions);

  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet api_perms = info->GetAll();
  for (ExtensionAPIPermissionSet::iterator i = api_perms.begin();
       i != api_perms.end(); ++i) {
    if (*i == ExtensionAPIPermission::kExperimental)
      continue;

    ExtensionAPIPermission* permission = info->GetByID(*i);
    const char* name = permission->name();
    StringValue* p = new StringValue(name);
    permissions->Clear();
    permissions->Append(p);

    if (!permission->is_component_only())
      continue;

    // Component-only extensions should only be enabled for component
    // extensions.
    LoadAndExpectError(Manifest(manifest.get(), name),
                       errors::kPermissionNotAllowed);
    LoadAndExpectSuccess(Manifest(manifest.get(), name),
                         Extension::COMPONENT);
  }
}

TEST_F(ExtensionManifestTest, AllowUnrecognizedPermissions) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("valid_app.json", &error));
  ListValue* permissions = NULL;
  ASSERT_TRUE(manifest->GetList("permissions", &permissions));
  permissions->Append(new StringValue("not-a-valid-permission"));
  LoadAndExpectSuccess(Manifest(manifest.get(), ""));
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

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("image/png", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_EQ("chrome-extension",
            extension->intents_services()[0].service_url.scheme());
  EXPECT_EQ("///services/share",
            extension->intents_services()[0].service_url.path());
  EXPECT_EQ("Sample Sharing Intent",
            UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
            extension->intents_services()[0].disposition);

  // Verify that optional fields are filled with defaults.
  extension = LoadAndExpectSuccess("intent_valid_minimal.json");
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_TRUE(extension->intents_services()[0].service_url.is_empty());
  EXPECT_EQ("", UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW,
            extension->intents_services()[0].disposition);
}

TEST_F(ExtensionManifestTest, ForbidPortsInPermissions) {
  // Loading as a user would shoud not trigger an error.
  LoadAndExpectSuccess("forbid_ports_in_permissions.json");

  // Ideally, loading as a developer would give an error.
  // To ensure that we do not error out on a valid permission
  // in a future version of chrome, validation is to loose
  // to flag this case.
  LoadAndExpectSuccess("forbid_ports_in_permissions.json",
                       Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);
}

TEST_F(ExtensionManifestTest, IsolatedApps) {
  // Requires --enable-experimental-extension-apis
  LoadAndExpectError("isolated_app_valid.json",
                     errors::kExperimentalFlagRequired);

  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("isolated_app_valid.json"));
  EXPECT_TRUE(extension2->is_storage_isolated());
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
  extension = LoadExtension(
      "filebrowser_url_override.json",
      &error,
      Extension::COMPONENT,
      Extension::STRICT_ERROR_CHECKS);
#if defined(FILE_MANAGER_EXTENSION)
  EXPECT_EQ("", error);
#else
  EXPECT_EQ(std::string(errors::kInvalidChromeURLOverrides), error);
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

TEST_F(ExtensionManifestTest, PlatformAppOnlyPermissions) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet private_perms;
  private_perms.insert(ExtensionAPIPermission::kSocket);

  ExtensionAPIPermissionSet perms = info->GetAll();
  int count = 0;
  for (ExtensionAPIPermissionSet::iterator i = perms.begin();
       i != perms.end(); ++i) {
    count += private_perms.count(*i);
    EXPECT_EQ(private_perms.count(*i) > 0,
              info->GetByID(*i)->is_platform_app_only());
  }
  EXPECT_EQ(1, count);

  // This guy should fail to load because he's requesting platform-app-only
  // permissions.
  LoadAndExpectError("evil_non_platform_app.json",
                     errors::kPermissionNotAllowed,
                     Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);

  // This guy is identical to the previous but doesn't ask for any
  // platform-app-only permissions. We should be able to load him and ask
  // questions about his permissions.
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("not_platform_app.json"));
  scoped_refptr<const ExtensionPermissionSet> permissions;
  permissions = extension->GetActivePermissions();
  EXPECT_FALSE(permissions->HasPlatformAppPermissions());
}
