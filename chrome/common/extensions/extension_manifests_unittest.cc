// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/json/json_file_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/extensions/extension_l10n_util.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "chrome/common/extensions/file_browser_handler.h"
#include "chrome/common/extensions/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/web_intent_service_data.h"

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
  // If filename is a relative path, LoadManifestFile will treat it relative to
  // the appropriate test directory.
  static DictionaryValue* LoadManifestFile(const std::string& filename,
                                           std::string* error) {
    FilePath filename_path(FilePath::FromUTF8Unsafe(filename));
    FilePath extension_path;
    FilePath manifest_path;

    if (filename_path.IsAbsolute()) {
      extension_path = filename_path.DirName();
      manifest_path = filename_path;
    } else {
      PathService::Get(chrome::DIR_TEST_DATA, &extension_path);
      extension_path = extension_path.AppendASCII("extensions")
          .AppendASCII("manifest_tests");
      manifest_path = extension_path.AppendASCII(filename.c_str());
    }

    EXPECT_TRUE(file_util::PathExists(manifest_path)) <<
        "Couldn't find " << manifest_path.value();

    JSONFileValueSerializer serializer(manifest_path);
    DictionaryValue* manifest =
        static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error));

    // Most unit tests don't need localization, and they'll fail if we try to
    // localize them, since their manifests don't have a default_locale key.
    // Only localize manifests that indicate they want to be localized.
    // Calling LocalizeExtension at this point mirrors
    // extension_file_util::LoadExtension.
    if (manifest && filename.find("localized") != std::string::npos)
      extension_l10n_util::LocalizeExtension(extension_path, manifest, error);

    return manifest;
  }

  // Helper class that simplifies creating methods that take either a filename
  // to a manifest or the manifest itself.
  class Manifest {
   public:
    explicit Manifest(const char* name)
        : name_(name), manifest_(NULL) {
    }
    Manifest(DictionaryValue* manifest, const char* name)
        : name_(name), manifest_(manifest) {
    }
    Manifest(const Manifest& m) {
      // C++98 requires the copy constructor for a type to be visible if you
      // take a const-ref of a temporary for that type.  Since Manifest
      // contains a scoped_ptr, its implicit copy constructor is declared
      // Manifest(Manifest&) according to spec 12.8.5.  This breaks the first
      // requirement and thus you cannot use it with LoadAndExpectError() or
      // LoadAndExpectSuccess() easily.
      //
      // To get around this spec pedantry, we declare the copy constructor
      // explicitly.  It will never get invoked.
      NOTREACHED();
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
  scoped_refptr<Extension> LoadAndExpectSuccess(
      const char* manifest_name,
      Extension::Location location = Extension::INTERNAL,
      int flags = Extension::NO_FLAGS) {
    return LoadAndExpectSuccess(Manifest(manifest_name), location, flags);
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
  void LoadAndExpectError(const char* manifest_name,
                          const std::string& expected_error,
                          Extension::Location location = Extension::INTERNAL,
                          int flags = Extension::NO_FLAGS) {
    return LoadAndExpectError(
        Manifest(manifest_name), expected_error, location, flags);
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
  LoadAndExpectError("init_valid_platform_app.json",
                     errors::kPlatformAppFlagRequired);

  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);

  LoadAndExpectSuccess("init_valid_platform_app.json");
}

TEST_F(ExtensionManifestTest, CertainApisRequirePlatformApps) {
  // Put APIs here that should be restricted to platform apps, but that haven't
  // yet graduated from experimental.
  const char* kPlatformAppExperimentalApis[] = {
    "dns",
    "serial",
    "socket",
  };
  // TODO(miket): When the first platform-app API leaves experimental, write
  // similar code that tests without the experimental flag.

  // This manifest is a skeleton used to build more specific manifests for
  // testing. The requirements are that (1) it be a valid platform app, and (2)
  // it contain no permissions dictionary.
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("init_valid_platform_app.json", &error));

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Create each manifest.
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];

    // DictionaryValue will take ownership of this ListValue.
    ListValue *permissions = new ListValue();
    permissions->Append(base::Value::CreateStringValue("experimental"));
    permissions->Append(base::Value::CreateStringValue(api_name));
    manifest->Set("permissions", permissions);

    // Each of these files lives in the scoped temp directory, so it will be
    // cleaned up at test teardown.
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    JSONFileValueSerializer serializer(file_path);
    serializer.Serialize(*(manifest.get()));
  }

  // First try to load without any flags. This should fail for every API.
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectError(file_path.MaybeAsASCII().c_str(),
                       errors::kExperimentalFlagRequired);
  }

  // Now try again with experimental flag set.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectError(file_path.MaybeAsASCII().c_str(),
                       errors::kPlatformAppFlagRequired);
  }

  // Finally, we should succeed with both experimental and platform-app flags
  // set.
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);
  for (size_t i = 0; i < arraysize(kPlatformAppExperimentalApis); ++i) {
    const char* api_name = kPlatformAppExperimentalApis[i];
    FilePath file_path = temp_dir.path().AppendASCII(api_name);
    LoadAndExpectSuccess(file_path.MaybeAsASCII().c_str());
  }
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

  LoadAndExpectSuccess("web_urls_has_port.json");

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
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValueContainer,
                         keys::kLaunchWidth));
  LoadAndExpectError("launch_width_negative.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWidth));
  LoadAndExpectError("launch_height_invalid.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValueContainer,
                         keys::kLaunchHeight));
  LoadAndExpectError("launch_height_negative.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchHeight));

  LoadAndExpectError("launch_min_width_invalid.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValueContainer,
                         keys::kLaunchMinWidth));
  LoadAndExpectError("launch_min_width_negative.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchMinWidth));
  LoadAndExpectError("launch_min_height_invalid.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValueContainer,
                         keys::kLaunchMinHeight));
  LoadAndExpectError("launch_min_height_negative.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchMinHeight));

  LoadAndExpectError("launch_container_missing_size_for_platform.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWidth));
  LoadAndExpectError("launch_container_invalid_size_constraints.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchMaxWidth));
}

TEST_F(ExtensionManifestTest, PlatformAppLaunchContainer) {
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnablePlatformApps);

  LoadAndExpectError("launch_container_invalid_type_for_platform.json",
                     errors::kInvalidLaunchContainerForPlatform);
}

TEST_F(ExtensionManifestTest, AppLaunchURL) {
  LoadAndExpectError("launch_path_and_url.json",
                     errors::kLaunchPathAndURLAreExclusive);
  LoadAndExpectError("launch_path_and_extent.json",
                     errors::kLaunchPathAndExtentAreExclusive);
  LoadAndExpectError("launch_path_invalid_type.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchLocalPath));
  LoadAndExpectError("launch_path_invalid_value.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchLocalPath));
  LoadAndExpectError("launch_path_invalid_localized.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchLocalPath));
  LoadAndExpectError("launch_url_invalid_type_1.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));
  LoadAndExpectError("launch_url_invalid_type_2.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));
  LoadAndExpectError("launch_url_invalid_type_3.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));
  LoadAndExpectError("launch_url_invalid_localized.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));

  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("launch_local_path.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  extension = LoadAndExpectSuccess("launch_local_path_localized.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  LoadAndExpectError("launch_web_url_relative.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));

  extension = LoadAndExpectSuccess("launch_web_url_absolute.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
  extension = LoadAndExpectSuccess("launch_web_url_localized.json");
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
      Manifest("permission_chrome_resources_url.json"),
      &error,
      Extension::COMPONENT,
      Extension::STRICT_ERROR_CHECKS);
  EXPECT_EQ("", error);
}

TEST_F(ExtensionManifestTest, ContentScriptMatchPattern) {
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

  LoadAndExpectSuccess("ports_in_content_scripts.json");
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
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
      errors::kInvalidDevToolsPage);

  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("devtools_extension.json");
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extension->devtools_url().spec());
  EXPECT_TRUE(extension->HasEffectiveAccessToAllHosts());
}

TEST_F(ExtensionManifestTest, BackgroundPermission) {
  LoadAndExpectError("background_permission.json",
                     errors::kBackgroundPermissionNeeded);

  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_permission_alias.json");
  EXPECT_TRUE(extension->HasAPIPermission(ExtensionAPIPermission::kBackground));
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
  ListValue* permissions = NULL;
  ASSERT_TRUE(manifest->GetList("permissions", &permissions));
  permissions->Append(new StringValue("not-a-valid-permission"));
  LoadAndExpectSuccess(Manifest(manifest.get(), ""));
}

TEST_F(ExtensionManifestTest, NormalizeIconPaths) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("normalize_icon_paths.json"));
  EXPECT_EQ("16.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_MEDIUM,
      ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, InvalidIconSizes) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("init_ignored_icon_size.json"));
  EXPECT_EQ("", extension->icons().Get(
      static_cast<ExtensionIconSet::Icons>(300),
      ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, ValidIconSizes) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("init_valid_icon_size.json"));
  EXPECT_EQ("16.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_BITTY, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("24.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_SMALLISH,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("32.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_SMALL, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("48.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_MEDIUM,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("128.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_LARGE, ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("256.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_EXTRA_LARGE,
      ExtensionIconSet::MATCH_EXACTLY));
  EXPECT_EQ("512.png", extension->icons().Get(
      ExtensionIconSet::EXTENSION_ICON_GIGANTOR,
      ExtensionIconSet::MATCH_EXACTLY));
}

TEST_F(ExtensionManifestTest, DisallowMultipleUISurfaces) {
  LoadAndExpectError("multiple_ui_surfaces.json", errors::kOneUISurfaceOnly);
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
  Testcase testcases[] = {
    {"intent_invalid_1.json", errors::kInvalidIntents},
    {"intent_invalid_2.json", errors::kInvalidIntent},
    {"intent_invalid_3.json", errors::kInvalidIntentHref},
    {"intent_invalid_4.json", errors::kInvalidIntentDisposition},
    {"intent_invalid_5.json", errors::kInvalidIntentType},
    {"intent_invalid_6.json", errors::kInvalidIntentTitle},
    {"intent_invalid_packaged_app.json", errors::kCannotAccessPage},
    {"intent_invalid_href_and_path.json",
        errors::kInvalidIntentHrefOldAndNewKey},
    {"intent_invalid_multi_href.json", errors::kInvalidIntent},
  };
  RunTestcases(testcases, arraysize(testcases));

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid.json"));
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("image/png", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_EQ("chrome-extension",
            extension->intents_services()[0].service_url.scheme());
  EXPECT_EQ("//services/share",
            extension->intents_services()[0].service_url.path());
  EXPECT_EQ("Sample Sharing Intent",
            UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
            extension->intents_services()[0].disposition);

  // Verify that optional fields are filled with defaults.
  extension = LoadAndExpectSuccess("intent_valid_minimal.json");
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("*", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("http://webintents.org/share",
            UTF16ToUTF8(extension->intents_services()[0].action));
  EXPECT_EQ("", UTF16ToUTF8(extension->intents_services()[0].title));
  EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_WINDOW,
            extension->intents_services()[0].disposition);

  // Make sure we support href instead of path.
  extension = LoadAndExpectSuccess("intent_valid_using_href.json");
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("//services/share",
      extension->intents_services()[0].service_url.path());
}

TEST_F(ExtensionManifestTest, WebIntentsWithMultipleMimeTypes) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multitype.json"));
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(2u, extension->intents_services().size());

  // One registration with multiple types generates a separate service for
  // each MIME type.
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ("http://webintents.org/share",
              UTF16ToUTF8(extension->intents_services()[i].action));
    EXPECT_EQ("chrome-extension",
              extension->intents_services()[i].service_url.scheme());
    EXPECT_EQ("//services/share",
              extension->intents_services()[i].service_url.path());
    EXPECT_EQ("Sample Sharing Intent",
              UTF16ToUTF8(extension->intents_services()[i].title));
    EXPECT_EQ(webkit_glue::WebIntentServiceData::DISPOSITION_INLINE,
              extension->intents_services()[i].disposition);
  }
  EXPECT_EQ("image/jpeg", UTF16ToUTF8(extension->intents_services()[0].type));
  EXPECT_EQ("image/bmp", UTF16ToUTF8(extension->intents_services()[1].type));

  LoadAndExpectError("intent_invalid_type_element.json",
                     extension_manifest_errors::kInvalidIntentTypeElement);
}

TEST_F(ExtensionManifestTest, WebIntentsInHostedApps) {
  LoadAndExpectError("intent_invalid_hosted_app_1.json",
                     errors::kInvalidIntentPageInHostedApp);
  LoadAndExpectError("intent_invalid_hosted_app_2.json",
                     errors::kInvalidIntentPageInHostedApp);
  LoadAndExpectError("intent_invalid_hosted_app_3.json",
                     errors::kInvalidIntentPageInHostedApp);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_hosted_app.json"));
  ASSERT_TRUE(extension.get() != NULL);

  ASSERT_EQ(3u, extension->intents_services().size());

  EXPECT_EQ("http://www.cfp.com/intents/edit.html",
            extension->intents_services()[0].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
            extension->intents_services()[1].service_url.spec());
  EXPECT_EQ("http://www.cloudfilepicker.com/intents/share.html",
            extension->intents_services()[2].service_url.spec());
}

TEST_F(ExtensionManifestTest, WebIntentsMultiHref) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_multi_href.json"));
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(2u, extension->intents_services().size());

  const std::vector<webkit_glue::WebIntentServiceData> &intents =
      extension->intents_services();

  EXPECT_EQ("chrome-extension", intents[0].service_url.scheme());
  EXPECT_EQ("//services/sharelink.html",intents[0].service_url.path());
  EXPECT_EQ("text/uri-list",UTF16ToUTF8(intents[0].type));

  EXPECT_EQ("chrome-extension", intents[1].service_url.scheme());
  EXPECT_EQ("//services/shareimage.html",intents[1].service_url.path());
  EXPECT_EQ("image/*",UTF16ToUTF8(intents[1].type));
}

TEST_F(ExtensionManifestTest, WebIntentsBlankHref) {
  LoadAndExpectError("intent_invalid_blank_action_extension.json",
                     errors::kInvalidIntentHrefEmpty);

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("intent_valid_blank_action_hosted.json"));
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("http://www.cloudfilepicker.com/",
      extension->intents_services()[0].service_url.spec());

  extension = LoadAndExpectSuccess("intent_valid_blank_action_packaged.json");
  ASSERT_TRUE(extension.get() != NULL);
  ASSERT_EQ(1u, extension->intents_services().size());
  EXPECT_EQ("chrome-extension",
      extension->intents_services()[0].service_url.scheme());
  EXPECT_EQ("/main.html",extension->intents_services()[0].service_url.path());
}

TEST_F(ExtensionManifestTest, PortsInPermissions) {
  // Loading as a user would shoud not trigger an error.
  LoadAndExpectSuccess("ports_in_permissions.json");
}

TEST_F(ExtensionManifestTest, WebAccessibleResources) {
  // Manifest version 2 with web accessible resources specified.
  scoped_refptr<Extension> extension1(
      LoadAndExpectSuccess("web_accessible_resources_1.json"));

  // Manifest version 2 with no web accessible resources.
  scoped_refptr<Extension> extension2(
      LoadAndExpectSuccess("web_accessible_resources_2.json"));

  // Default manifest version with web accessible resources specified.
  scoped_refptr<Extension> extension3(
      LoadAndExpectSuccess("web_accessible_resources_3.json"));

  // Default manifest version with no web accessible resources.
  scoped_refptr<Extension> extension4(
      LoadAndExpectSuccess("web_accessible_resources_4.json"));

  EXPECT_TRUE(extension1->HasWebAccessibleResources());
  EXPECT_FALSE(extension2->HasWebAccessibleResources());
  EXPECT_TRUE(extension3->HasWebAccessibleResources());
  EXPECT_FALSE(extension4->HasWebAccessibleResources());

  EXPECT_TRUE(extension1->IsResourceWebAccessible("/test"));
  EXPECT_FALSE(extension1->IsResourceWebAccessible("/none"));

  EXPECT_FALSE(extension2->IsResourceWebAccessible("/test"));

  EXPECT_TRUE(extension3->IsResourceWebAccessible("/test"));
  EXPECT_FALSE(extension3->IsResourceWebAccessible("/none"));

  EXPECT_TRUE(extension4->IsResourceWebAccessible("/test"));
  EXPECT_TRUE(extension4->IsResourceWebAccessible("/none"));
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
  LoadAndExpectError("filebrowser_invalid_access_permission.json",
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kInvalidFileAccessValue, base::IntToString(1)));
  LoadAndExpectError("filebrowser_invalid_access_permission_list.json",
      errors::kInvalidFileAccessList);
  LoadAndExpectError("filebrowser_invalid_empty_access_permission_list.json",
      errors::kInvalidFileAccessList);
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
  ASSERT_FALSE(action->HasCreateAccessPermission());
  ASSERT_TRUE(action->CanRead());
  ASSERT_TRUE(action->CanWrite());

  scoped_refptr<Extension> create_extension(
      LoadAndExpectSuccess("filebrowser_valid_with_create.json"));
  ASSERT_TRUE(create_extension->file_browser_handlers() != NULL);
  ASSERT_EQ(create_extension->file_browser_handlers()->size(), 1U);
  const FileBrowserHandler* create_action =
      create_extension->file_browser_handlers()->at(0).get();
  EXPECT_EQ(create_action->title(), "Default title");
  EXPECT_EQ(create_action->icon_path(), "icon.png");
  const URLPatternSet& create_patterns = create_action->file_url_patterns();
  ASSERT_EQ(create_patterns.patterns().size(), 0U);
  ASSERT_TRUE(create_action->HasCreateAccessPermission());
  ASSERT_FALSE(create_action->CanRead());
  ASSERT_FALSE(create_action->CanWrite());
}

TEST_F(ExtensionManifestTest, FileManagerURLOverride) {
  // A component extention can override chrome://files/ URL.
  std::string error;
  scoped_refptr<Extension> extension;
  extension = LoadExtension(
      Manifest("filebrowser_url_override.json"),
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

TEST_F(ExtensionManifestTest, BackgroundPage) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("background_page.json"));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", extension->GetBackgroundURL().path());
  EXPECT_TRUE(extension->allow_background_js_access());

  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("background_page_legacy.json", &error));
  ASSERT_TRUE(manifest.get());
  extension = LoadAndExpectSuccess(Manifest(manifest.get(), ""));
  ASSERT_TRUE(extension);
  EXPECT_EQ("/foo.html", extension->GetBackgroundURL().path());

  manifest->SetInteger(keys::kManifestVersion, 2);
  LoadAndExpectError(
      Manifest(manifest.get(), ""),
      ExtensionErrorUtils::FormatErrorMessage(
          errors::kFeatureNotAllowed, "background_page"),
      Extension::INTERNAL, Extension::STRICT_ERROR_CHECKS);
}

TEST_F(ExtensionManifestTest, BackgroundScripts) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("background_scripts.json", &error));
  ASSERT_TRUE(manifest.get());

  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess(Manifest(manifest.get(), "")));
  ASSERT_TRUE(extension);
  EXPECT_EQ(2u, extension->background_scripts().size());
  EXPECT_EQ("foo.js", extension->background_scripts()[0u]);
  EXPECT_EQ("bar/baz.js", extension->background_scripts()[1u]);

  EXPECT_TRUE(extension->has_background_page());
  EXPECT_EQ(std::string("/") +
            extension_filenames::kGeneratedBackgroundPageFilename,
            extension->GetBackgroundURL().path());

  manifest->SetString("background_page", "monkey.html");
  LoadAndExpectError(Manifest(manifest.get(), ""),
                     errors::kInvalidBackgroundCombination);
}

TEST_F(ExtensionManifestTest, BackgroundAllowNoJsAccess) {
  scoped_refptr<Extension> extension;
  extension = LoadAndExpectSuccess("background_allow_no_js_access.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->allow_background_js_access());

  extension = LoadAndExpectSuccess("background_allow_no_js_access2.json");
  ASSERT_TRUE(extension);
  EXPECT_FALSE(extension->allow_background_js_access());
}

TEST_F(ExtensionManifestTest, PageActionManifestVersion2) {
  scoped_refptr<Extension> extension(
      LoadAndExpectSuccess("page_action_manifest_version_2.json"));
  ASSERT_TRUE(extension.get());
  ASSERT_TRUE(extension->page_action());

  EXPECT_EQ("", extension->page_action()->id());
  EXPECT_EQ(0u, extension->page_action()->icon_paths()->size());
  EXPECT_EQ("", extension->page_action()->GetTitle(
      ExtensionAction::kDefaultTabId));
  EXPECT_FALSE(extension->page_action()->HasPopup(
      ExtensionAction::kDefaultTabId));

  LoadAndExpectError("page_action_manifest_version_2b.json",
                     errors::kInvalidPageActionPopup);
}

TEST_F(ExtensionManifestTest, StorageAPIManifestVersionAvailability) {
  DictionaryValue base_manifest;
  {
    base_manifest.SetString(keys::kName, "test");
    base_manifest.SetString(keys::kVersion, "0.1");
    ListValue* permissions = new ListValue();
    permissions->Append(Value::CreateStringValue("storage"));
    base_manifest.Set(keys::kPermissions, permissions);
  }

  std::string kManifestVersionError("Requires manifest version of at least 2.");

  // Extension with no manifest version cannot use storage API.
  {
    Manifest manifest(&base_manifest, "test");
    LoadAndExpectError(manifest, kManifestVersionError);
  }

  // Extension with manifest version 1 cannot use storage API.
  {
    DictionaryValue manifest_with_version;
    manifest_with_version.SetInteger(keys::kManifestVersion, 1);
    manifest_with_version.MergeDictionary(&base_manifest);

    Manifest manifest(&manifest_with_version, "test");
    LoadAndExpectError(manifest, kManifestVersionError);
  }

  // Extension with manifest version 2 *can* use storage API.
  {
    DictionaryValue manifest_with_version;
    manifest_with_version.SetInteger(keys::kManifestVersion, 2);
    manifest_with_version.MergeDictionary(&base_manifest);

    Manifest manifest(&manifest_with_version, "test");
    LoadAndExpectSuccess(manifest);
  }
}
