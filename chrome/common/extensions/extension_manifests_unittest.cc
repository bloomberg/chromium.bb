// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
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
    FilePath path;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("extensions")
        .AppendASCII("manifest_tests")
        .AppendASCII(name.c_str());
    EXPECT_TRUE(file_util::PathExists(path));

    JSONFileValueSerializer serializer(path);
    scoped_ptr<DictionaryValue> value(
        static_cast<DictionaryValue*>(serializer.Deserialize(error)));
    if (!value.get())
      return NULL;

    scoped_ptr<Extension> extension(new Extension(path.DirName()));
    if (enable_apps_)
      extension->set_apps_enabled(true);

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
    EXPECT_EQ(expected_error, error);
  }

  bool enable_apps_;
};

TEST_F(ManifestTest, AppsDisabledByDefault) {
  enable_apps_ = false;
  LoadAndExpectError("web_content_disabled.json", errors::kAppsNotEnabled);
  LoadAndExpectError("launch_local_path.json", errors::kAppsNotEnabled);
}

TEST_F(ManifestTest, ValidApp) {
  scoped_ptr<Extension> extension(LoadAndExpectSuccess("valid_app.json"));
  EXPECT_TRUE(extension->web_content_enabled());
  EXPECT_EQ(GURL("http://www.google.com/"), extension->web_extent().origin());
  EXPECT_EQ(2u, extension->web_extent().paths().size());
  EXPECT_EQ("mail/", extension->web_extent().paths()[0]);
  EXPECT_EQ("foobar/", extension->web_extent().paths()[1]);
  EXPECT_EQ(Extension::LAUNCH_WINDOW, extension->launch_container());
  EXPECT_EQ("mail/", extension->launch_web_url());
}

TEST_F(ManifestTest, AppWebContentEnabled) {
  LoadAndExpectError("web_content_enabled_invalid.json",
                     errors::kInvalidWebContentEnabled);
  LoadAndExpectError("web_content_disabled.json",
                     errors::kWebContentMustBeEnabled);
  LoadAndExpectError("web_content_not_enabled.json",
                     errors::kWebContentMustBeEnabled);
}

TEST_F(ManifestTest, AppWebOrigin) {
  LoadAndExpectError("web_origin_wrong_type.json",
                     errors::kInvalidWebOrigin);
  LoadAndExpectError("web_origin_invalid_1.json",
                     errors::kInvalidWebOrigin);
  LoadAndExpectError("web_origin_invalid_2.json",
                     errors::kInvalidWebOrigin);
  LoadAndExpectError("web_origin_invalid_3.json",
                     errors::kInvalidWebOrigin);
}

TEST_F(ManifestTest, AppWebPaths) {
  LoadAndExpectError("web_paths_wrong_type.json",
                     errors::kInvalidWebPaths);
  LoadAndExpectError("web_paths_invalid_path_1.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebPath, "0"));
  LoadAndExpectError("web_paths_invalid_path_2.json",
                     ExtensionErrorUtils::FormatErrorMessage(
                         errors::kInvalidWebPath, "0"));
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

  LoadAndExpectError("launch_container_invalid_type.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_invalid_value.json",
                     errors::kInvalidLaunchContainer);
  LoadAndExpectError("launch_container_without_launch_url.json",
                     errors::kLaunchContainerWithoutURL);
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

  extension.reset(LoadAndExpectSuccess("launch_web_url_relative.json"));
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());

  extension.reset(LoadAndExpectSuccess("launch_web_url_absolute.json"));
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
}
