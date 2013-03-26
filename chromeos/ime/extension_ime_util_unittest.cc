// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/extension_ime_util.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(ExtensionIMEUtilTest, GetInputMethodIDTest) {
  EXPECT_EQ("_ext_ime_ABCDE12345",
            extension_ime_util::GetInputMethodID("ABCDE", "12345"));
}

TEST(ExtensionIMEUtilTest, IsExtensionIMETest) {
  EXPECT_TRUE(extension_ime_util::IsExtensionIME(
      extension_ime_util::GetInputMethodID("abcde", "12345")));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME(""));
  EXPECT_FALSE(extension_ime_util::IsExtensionIME("mozc"));
}

}  // namespace chromeos
