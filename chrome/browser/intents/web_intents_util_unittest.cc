// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_util.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

bool IsRecognized(const std::string& value) {
  return web_intents::IsRecognizedAction(ASCIIToUTF16(value));
}

}  // namespace

namespace web_intents {

TEST(WebIntentsUtilTest, GetRecognizedAction) {
  EXPECT_TRUE(IsRecognized(action::kShare));
  EXPECT_TRUE(IsRecognized(action::kEdit));
  EXPECT_TRUE(IsRecognized(action::kView));
  EXPECT_TRUE(IsRecognized(action::kPick));
  EXPECT_TRUE(IsRecognized(action::kSubscribe));
  EXPECT_TRUE(IsRecognized(action::kSave));
}

TEST(WebIntentsUtilTest, GetRecognizedActionFailure) {
  EXPECT_FALSE(IsRecognized(std::string(action::kPick) + "lezooka"));
  EXPECT_FALSE(IsRecognized("Chrome LAX"));
  EXPECT_FALSE(IsRecognized("_zoom "));
  EXPECT_FALSE(IsRecognized("  "));
  EXPECT_FALSE(IsRecognized(""));

  // case variation is intentionally not supported
  EXPECT_FALSE(IsRecognized("http://webintents.org/Share"));
  EXPECT_FALSE(IsRecognized("http://webintents.org/eDit"));
  EXPECT_FALSE(IsRecognized("http://webintents.org/viEw"));
  EXPECT_FALSE(IsRecognized("http://webintents.org/picK"));
  EXPECT_FALSE(IsRecognized("http://webintents.org/subsCribe"));
  EXPECT_FALSE(IsRecognized("http://webintents.org/SAVE"));
}

}
