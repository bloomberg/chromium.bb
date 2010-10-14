// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

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

  Extension* LoadExtensionWithLocation(DictionaryValue* value,
                                       Extension::Location location,
                                       std::string* error) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions").AppendASCII("manifest_tests");

    scoped_ptr<Extension> extension(new Extension(path.DirName()));
    extension->set_location(location);

    if (!extension->InitFromValue(*value, false, error))
      return NULL;

    return extension.release();
  }

  Extension* LoadExtension(const std::string& name,
                           std::string* error) {
    return LoadExtensionWithLocation(name, Extension::INTERNAL, error);
  }

  Extension* LoadExtension(DictionaryValue* value,
                           std::string* error) {
    return LoadExtensionWithLocation(value, Extension::INTERNAL, error);
  }

  Extension* LoadExtensionWithLocation(const std::string& name,
                                       Extension::Location location,
                                       std::string* error) {
    scoped_ptr<DictionaryValue> value(LoadManifestFile(name, error));
    if (!value.get())
      return NULL;
    return LoadExtensionWithLocation(value.get(), location, error);
  }

  Extension* LoadAndExpectSuccess(const std::string& name) {
    std::string error;
    Extension* extension = LoadExtension(name, &error);
    EXPECT_TRUE(extension) << name;
    EXPECT_EQ("", error) << name;
    return extension;
  }

  Extension* LoadAndExpectSuccess(DictionaryValue* manifest,
                                  const std::string& name) {
    std::string error;
    Extension* extension = LoadExtension(manifest, &error);
    EXPECT_TRUE(extension) << "Unexpected success for " << name;
    EXPECT_EQ("", error) << "Unexpected no error for " << name;
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
    scoped_ptr<Extension> extension(LoadExtension(name, &error));
    VerifyExpectedError(extension.get(), name, error, expected_error);
  }

  void LoadAndExpectError(DictionaryValue* manifest,
                          const std::string& name,
                          const std::string& expected_error) {
    std::string error;
    scoped_ptr<Extension> extension(LoadExtension(manifest, &error));
    VerifyExpectedError(extension.get(), name, error, expected_error);
  }

  bool enable_apps_;
};

TEST_F(ExtensionManifestTest, ValidApp) {
  scoped_ptr<Extension> extension(LoadAndExpectSuccess("valid_app.json"));
  ASSERT_EQ(2u, extension->web_extent().patterns().size());
  EXPECT_EQ("http://www.google.com/mail/*",
            extension->web_extent().patterns()[0].GetAsString());
  EXPECT_EQ("http://www.google.com/foobar/*",
            extension->web_extent().patterns()[1].GetAsString());
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());
  EXPECT_EQ("http://www.google.com/mail/", extension->launch_web_url());
}

TEST_F(ExtensionManifestTest, AppWebUrls) {
  LoadAndExpectError("web_urls_wrong_type.json",
                     errors::kInvalidWebURLs);
  LoadAndExpectError("web_urls_invalid_1.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebURL, "0"));
  LoadAndExpectError("web_urls_invalid_2.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebURL, "0"));
  LoadAndExpectError("web_urls_invalid_3.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebURL, "0"));
  LoadAndExpectError("web_urls_invalid_4.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebURL, "0"));

  scoped_ptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns()[0].GetAsString());
}

TEST_F(ExtensionManifestTest, AppLaunchContainer) {
  scoped_ptr<Extension> extension;

  extension.reset(LoadAndExpectSuccess("launch_tab.json"));
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_panel.json"));
  EXPECT_EQ(extension_misc::LAUNCH_PANEL, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_default.json"));
  EXPECT_EQ(extension_misc::LAUNCH_TAB, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_width.json"));
  EXPECT_EQ(640, extension->launch_width());

  extension.reset(LoadAndExpectSuccess("launch_height.json"));
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
  LoadAndExpectError("launch_path_invalid_type.json",
                     errors::kInvalidLaunchLocalPath);
  LoadAndExpectError("launch_path_invalid_value.json",
                     errors::kInvalidLaunchLocalPath);
  LoadAndExpectError("launch_url_invalid_type.json",
                     errors::kInvalidLaunchWebURL);

  scoped_ptr<Extension> extension;
  extension.reset(LoadAndExpectSuccess("launch_local_path.json"));
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  LoadAndExpectError("launch_web_url_relative.json",
                     errors::kInvalidLaunchWebURL);

  extension.reset(LoadAndExpectSuccess("launch_web_url_absolute.json"));
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
}

TEST_F(ExtensionManifestTest, Override) {
  LoadAndExpectError("override_newtab_and_history.json",
                     errors::kMultipleOverrides);
  LoadAndExpectError("override_invalid_page.json",
                     errors::kInvalidChromeURLOverrides);

  scoped_ptr<Extension> extension;

  extension.reset(LoadAndExpectSuccess("override_new_tab.json"));
  EXPECT_EQ(extension->url().spec() + "newtab.html",
            extension->GetChromeURLOverrides().find("newtab")->second.spec());

  extension.reset(LoadAndExpectSuccess("override_history.json"));
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
  scoped_ptr<Extension> extension;
  extension.reset(LoadExtensionWithLocation(
      "permission_chrome_resources_url.json",
      Extension::COMPONENT,
      &error));
  EXPECT_EQ("", error);
}

TEST_F(ExtensionManifestTest, ChromeURLContentScriptInvalid) {
  LoadAndExpectError("content_script_chrome_url_invalid.json",
      errors::kInvalidMatch);
}

TEST_F(ExtensionManifestTest, DevToolsExtensions) {
  LoadAndExpectError("devtools_extension_no_permissions.json",
      errors::kDevToolsExperimental);
  LoadAndExpectError("devtools_extension_url_invalid_type.json",
      errors::kInvalidDevToolsPage);

  CommandLine old_command_line = *CommandLine::ForCurrentProcess();
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  scoped_ptr<Extension> extension;
  extension.reset(LoadAndExpectSuccess("devtools_extension.json"));
  EXPECT_EQ(extension->url().spec() + "devtools.html",
            extension->devtools_url().spec());
  *CommandLine::ForCurrentProcess() = old_command_line;
}

TEST_F(ExtensionManifestTest, DisallowHybridApps) {
  LoadAndExpectError("disallow_hybrid_1.json",
                     errors::kHostedAppsCannotIncludeExtensionFeatures);
  LoadAndExpectError("disallow_hybrid_2.json",
                     errors::kHostedAppsCannotIncludeExtensionFeatures);
}

TEST_F(ExtensionManifestTest, OptionsPageInApps) {
  scoped_ptr<Extension> extension;

  // Allow options page with absolute URL in hosed apps.
  extension.reset(
      LoadAndExpectSuccess("hosted_app_absolute_options.json"));
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

TEST_F(ExtensionManifestTest, DisallowExtensionPermissions) {
  std::string error;
  scoped_ptr<DictionaryValue> manifest(
      LoadManifestFile("valid_app.json", &error));
  ASSERT_TRUE(manifest.get());

  ListValue *permissions = new ListValue();
  manifest->Set(keys::kPermissions, permissions);
  for (size_t i = 0; i < Extension::kNumPermissions; i++) {
    const char* name = Extension::kPermissions[i].name;
    StringValue* p = new StringValue(name);
    permissions->Clear();
    permissions->Append(p);
    std::string message_name = StringPrintf("permission-%s", name);
    if (Extension::IsHostedAppPermission(name)) {
      scoped_ptr<Extension> extension;
      extension.reset(LoadAndExpectSuccess(manifest.get(), message_name));
    } else {
      LoadAndExpectError(manifest.get(), message_name,
                         errors::kInvalidPermission);
    }
  }
}

TEST_F(ExtensionManifestTest, NormalizeIconPaths) {
  scoped_ptr<Extension> extension(
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
