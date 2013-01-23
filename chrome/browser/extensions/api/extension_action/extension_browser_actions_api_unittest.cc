// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/values.h"
#include "chrome/browser/extensions/api/extension_action/extension_action_api.h"
#include "chrome/common/extensions/extension.h"
#include "third_party/skia/include/core/SkColor.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

void RunPassTest(const std::string& css_string, SkColor expected_result) {
  SkColor color = 0;
  EXPECT_TRUE(
      ExtensionActionFunction::ParseCSSColorString(css_string, &color));
  EXPECT_EQ(color, expected_result);
}

void RunFailTest(const std::string& css_string) {
  SkColor color = 0;
  EXPECT_FALSE(
      ExtensionActionFunction::ParseCSSColorString(css_string, &color));
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundNormalCSS) {
  RunPassTest("#34006A", SkColorSetARGB(0xFF, 0x34, 0, 0x6A));
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundShortCSS) {
  RunPassTest("#A1E", SkColorSetARGB(0xFF, 0xAA, 0x11, 0xEE));
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundCSSNoHash) {
  RunFailTest("11FF22");
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundCSSTooShort) {
  RunFailTest("#FF22");
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundCSSTooLong) {
  RunFailTest("#FF22128");
}

TEST(ExtensionBrowserActionsApiTest, ChangeBadgeBackgroundCSSInvalid) {
  RunFailTest("#-22128");
}

}  // namespace extensions
