// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/web_intents_util.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_intents {
namespace {

bool IsRecognized(const std::string& value) {
  return web_intents::IsRecognizedAction(ASCIIToUTF16(value));
}

ActionId ToAction(const std::string& value) {
  return web_intents::ToActionId(ASCIIToUTF16(value));
}

bool TypesMatch(const std::string& a, const std::string& b) {
  return MimeTypesMatch(ASCIIToUTF16(a), ASCIIToUTF16(b));
}

}  // namespace

TEST(WebIntentsUtilTest, IsRecognizedAction) {
  EXPECT_TRUE(IsRecognized(kActionEdit));
  EXPECT_FALSE(IsRecognized("http://webintents.org/eDit"));  // case matters
  EXPECT_TRUE(IsRecognized(kActionPick));
  EXPECT_TRUE(IsRecognized(kActionSave));
  EXPECT_TRUE(IsRecognized(kActionShare));
  EXPECT_TRUE(IsRecognized(kActionSubscribe));
  EXPECT_TRUE(IsRecognized(kActionView));
}

TEST(WebIntentsUtilTest, IsRecognizedActionFailure) {
  EXPECT_FALSE(IsRecognized(std::string(kActionPick) + "lezooka"));
  EXPECT_FALSE(IsRecognized("Chrome LAX"));
  EXPECT_FALSE(IsRecognized("_zoom "));
  EXPECT_FALSE(IsRecognized("  "));
  EXPECT_FALSE(IsRecognized(""));
}

TEST(WebIntentsUtilTest, ToActionId) {
  EXPECT_EQ(ACTION_ID_EDIT, ToAction(kActionEdit));
  EXPECT_EQ(ACTION_ID_PICK, ToAction(kActionPick));
  EXPECT_EQ(ACTION_ID_SAVE, ToAction(kActionSave));
  EXPECT_EQ(ACTION_ID_SHARE, ToAction(kActionShare));
  EXPECT_EQ(ACTION_ID_SUBSCRIBE, ToAction(kActionSubscribe));
  EXPECT_EQ(ACTION_ID_VIEW, ToAction(kActionView));
}

TEST(WebIntentsUtilTest, MimeTypesMatchLiteral) {
  EXPECT_TRUE(TypesMatch("image/png", "image/png"));

  EXPECT_FALSE(TypesMatch(" image/png", "image/png"));
  EXPECT_FALSE(TypesMatch("image/png", " image/png"));
  EXPECT_FALSE(TypesMatch("image/png ", "image/png"));
  EXPECT_FALSE(TypesMatch("image/png", "image/png "));
  EXPECT_FALSE(TypesMatch("image/jpg", "image/png"));
  EXPECT_FALSE(TypesMatch("image/png", "image/jpg"));
}

TEST(WebIntentsUtilTest, MimeTypesMatchWildCards) {
  EXPECT_TRUE(TypesMatch("*", "*"));
  EXPECT_TRUE(TypesMatch("*", "*/*"));
  EXPECT_TRUE(TypesMatch("*/*", "*"));
  EXPECT_TRUE(TypesMatch("*/*", "*/*"));
  EXPECT_TRUE(TypesMatch("*", "image/png"));
  EXPECT_TRUE(TypesMatch("image/png", "*"));
  EXPECT_TRUE(TypesMatch("*/*", "image/png"));
  EXPECT_TRUE(TypesMatch("image/png", "*/*"));

  EXPECT_FALSE(TypesMatch(" */*", "image/png"));
  EXPECT_FALSE(TypesMatch("*/* ", "image/png"));
  EXPECT_FALSE(TypesMatch("**", "image/png"));
}

}  // namepsace web_intents
