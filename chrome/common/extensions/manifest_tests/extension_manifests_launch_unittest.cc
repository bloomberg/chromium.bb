// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_tests/extension_manifest_test.h"

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_manifest_constants.h"
#include "extensions/common/error_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace errors = extension_manifest_errors;
namespace keys = extension_manifest_keys;

using extensions::ErrorUtils;

TEST_F(ExtensionManifestTest, AppLaunchContainer) {
  scoped_refptr<extensions::Extension> extension;

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

  Testcase testcases[] = {
    Testcase("launch_window.json", errors::kInvalidLaunchContainer),
    Testcase("launch_container_invalid_type.json",
             errors::kInvalidLaunchContainer),
    Testcase("launch_container_invalid_value.json",
             errors::kInvalidLaunchContainer),
    Testcase("launch_container_without_launch_url.json",
             errors::kLaunchURLRequired),
    Testcase("launch_width_invalid.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValueContainer,
                 keys::kLaunchWidth)),
    Testcase("launch_width_negative.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchWidth)),
    Testcase("launch_height_invalid.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValueContainer,
                 keys::kLaunchHeight)),
    Testcase("launch_height_negative.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchHeight))
  };
  RunTestcases(testcases, arraysize(testcases),
      EXPECT_TYPE_ERROR);
}

TEST_F(ExtensionManifestTest, AppLaunchURL) {
  Testcase testcases[] = {
    Testcase("launch_path_and_url.json",
             errors::kLaunchPathAndURLAreExclusive),
    Testcase("launch_path_and_extent.json",
             errors::kLaunchPathAndExtentAreExclusive),
    Testcase("launch_path_invalid_type.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchLocalPath)),
    Testcase("launch_path_invalid_value.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchLocalPath)),
    Testcase("launch_path_invalid_localized.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchLocalPath)),
    Testcase("launch_url_invalid_type_1.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchWebURL)),
    Testcase("launch_url_invalid_type_2.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchWebURL)),
    Testcase("launch_url_invalid_type_3.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchWebURL)),
    Testcase("launch_url_invalid_localized.json",
             ErrorUtils::FormatErrorMessage(
                 errors::kInvalidLaunchValue,
                 keys::kLaunchWebURL))
  };
  RunTestcases(testcases, arraysize(testcases),
      EXPECT_TYPE_ERROR);

  scoped_refptr<extensions::Extension> extension;
  extension = LoadAndExpectSuccess("launch_local_path.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  extension = LoadAndExpectSuccess("launch_local_path_localized.json");
  EXPECT_EQ(extension->url().spec() + "launch.html",
            extension->GetFullLaunchURL().spec());

  LoadAndExpectError("launch_web_url_relative.json",
                     ErrorUtils::FormatErrorMessage(
                         errors::kInvalidLaunchValue,
                         keys::kLaunchWebURL));

  extension = LoadAndExpectSuccess("launch_web_url_absolute.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
  extension = LoadAndExpectSuccess("launch_web_url_localized.json");
  EXPECT_EQ(GURL("http://www.google.com/launch.html"),
            extension->GetFullLaunchURL());
}
