// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "chrome/common/json_value_serializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;

class ManifestTest : public testing::Test {
 public:
  ManifestTest() : enable_apps_(true) {}

 protected:
  Extension* LoadExtension(const std::string& name,
                           std::string* error) {
    return LoadExtensionWithLocation(name, Extension::INTERNAL, error);
  }

  Extension* LoadExtensionWithLocation(const std::string& name,
                                       Extension::Location location,
                                       std::string* error) {
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions")
        .AppendASCII("manifest_tests")
        .AppendASCII(name.c_str());
    EXPECT_TRUE(file_util::PathExists(path));

    JSONFileValueSerializer serializer(path);
    scoped_ptr<DictionaryValue> value(
        static_cast<DictionaryValue*>(serializer.Deserialize(NULL, error)));
    if (!value.get())
      return NULL;

    scoped_ptr<Extension> extension(new Extension(path.DirName()));
    extension->set_location(location);
    extension->set_apps_enabled(enable_apps_);

    if (!extension->InitFromValue(*value, false, error))
      return NULL;

    return extension.release();
  }

  Extension* LoadAndExpectSuccess(const std::string& name) {
    std::string error;
    Extension* extension = LoadExtension(name, &error);
    EXPECT_TRUE(extension);
    EXPECT_EQ("", error);
    return extension;
  }

  void LoadAndExpectError(const std::string& name,
                          const std::string& expected_error) {
    std::string error;
    scoped_ptr<Extension> extension(LoadExtension(name, &error));
    EXPECT_FALSE(extension.get()) <<
        "Expected failure loading extension '" << name <<
        "', but didn't get one.";
    EXPECT_TRUE(MatchPatternASCII(error, expected_error)) << name <<
        " expected '" << expected_error << "' but got '" << error << "'";
  }

  bool enable_apps_;
};

TEST_F(ManifestTest, AppsDisabledByDefault) {
#if defined(OS_CHROMEOS)
  // On ChromeOS, apps are enabled by default.
  if (Extension::AppsAreEnabled())
    return;
#endif

  enable_apps_ = false;
  LoadAndExpectError("launch_local_path.json", errors::kAppsNotEnabled);
}

TEST_F(ManifestTest, ValidApp) {
  scoped_ptr<Extension> extension(LoadAndExpectSuccess("valid_app.json"));
  ASSERT_EQ(2u, extension->web_extent().patterns().size());
  EXPECT_EQ("http://www.google.com/mail/*",
            extension->web_extent().patterns()[0].GetAsString());
  EXPECT_EQ("http://www.google.com/foobar/*",
            extension->web_extent().patterns()[1].GetAsString());
  EXPECT_EQ(Extension::LAUNCH_WINDOW, extension->launch_container());
  EXPECT_EQ(false, extension->launch_fullscreen());
  EXPECT_EQ("http://www.google.com/mail/", extension->launch_web_url());
}

TEST_F(ManifestTest, AppWebUrls) {
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

  scoped_ptr<Extension> extension(
      LoadAndExpectSuccess("web_urls_default.json"));
  ASSERT_EQ(1u, extension->web_extent().patterns().size());
  EXPECT_EQ("*://www.google.com/*",
            extension->web_extent().patterns()[0].GetAsString());
}

TEST_F(ManifestTest, AppLaunchContainer) {
  scoped_ptr<Extension> extension;

  extension.reset(LoadAndExpectSuccess("launch_tab.json"));
  EXPECT_EQ(Extension::LAUNCH_TAB, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_window.json"));
  EXPECT_EQ(Extension::LAUNCH_WINDOW, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_panel.json"));
  EXPECT_EQ(Extension::LAUNCH_PANEL, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_default.json"));
  EXPECT_EQ(Extension::LAUNCH_TAB, extension->launch_container());

  extension.reset(LoadAndExpectSuccess("launch_fullscreen.json"));
  EXPECT_EQ(true, extension->launch_fullscreen());

  extension.reset(LoadAndExpectSuccess("launch_width.json"));
  EXPECT_EQ(640, extension->launch_width());

  extension.reset(LoadAndExpectSuccess("launch_height.json"));
  EXPECT_EQ(480, extension->launch_height());

  LoadAndExpectError("launch_container_invalid_type.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_invalid_value.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_without_launch_url.json",
                     errors::kLaunchURLRequired);
  LoadAndExpectError("launch_fullscreen_invalid.json",
                     errors::kInvalidLaunchFullscreen);
  LoadAndExpectError("launch_width_invalid.json",
                     errors::kInvalidLaunchWidthContainer);
  LoadAndExpectError("launch_width_negative.json",
                     errors::kInvalidLaunchWidth);
  LoadAndExpectError("launch_height_invalid.json",
                     errors::kInvalidLaunchHeightContainer);
  LoadAndExpectError("launch_height_negative.json",
                     errors::kInvalidLaunchHeight);
}

TEST_F(ManifestTest, AppLaunchURL) {
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

TEST_F(ManifestTest, Override) {
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

TEST_F(ManifestTest, ChromeURLPermissionInvalid) {
  LoadAndExpectError("permission_chrome_url_invalid.json",
      errors::kInvalidPermissionScheme);
}

TEST_F(ManifestTest, ChromeResourcesPermissionValidOnlyForComponents) {
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

TEST_F(ManifestTest, ChromeURLContentScriptInvalid) {
  LoadAndExpectError("content_script_chrome_url_invalid.json",
      errors::kInvalidMatch);
}
