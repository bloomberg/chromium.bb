// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"

#include "chromeos/services/ime/rulebased/controller.h"
#include "chromeos/services/ime/rulebased/rules_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace ime {

class RulebasedImeTest : public testing::Test {
 protected:
  RulebasedImeTest() = default;
  ~RulebasedImeTest() override = default;

  // testing::Test:
  void SetUp() override { controller_.reset(new rulebased::Controller); }

  std::unique_ptr<rulebased::Controller> controller_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RulebasedImeTest);
};

TEST_F(RulebasedImeTest, Arabic) {
  controller_->Activate("ar");

  rulebased::ProcessKeyResult res =
      controller_->ProcessKey("KeyA", rulebased::MODIFIER_SHIFT);
  EXPECT_TRUE(res.key_handled);
  std::string expected_str = base::WideToUTF8(L"\u0650");
  EXPECT_EQ(expected_str, res.commit_text);

  res = controller_->ProcessKey("KeyB", 0);
  EXPECT_TRUE(res.key_handled);
  expected_str = base::WideToUTF8(L"\u0644\u0627");
  EXPECT_EQ(expected_str, res.commit_text);

  res = controller_->ProcessKey("Space", 0);
  EXPECT_TRUE(res.key_handled);
  EXPECT_EQ(" ", res.commit_text);
}

TEST_F(RulebasedImeTest, ParseKeyMap) {
  // Empty.
  rulebased::KeyMap key_map = rulebased::ParseKeyMapForTesting(L"", false);
  EXPECT_TRUE(key_map.empty());

  // Single char mapping.
  key_map = rulebased::ParseKeyMapForTesting(L"abcde", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);

  // Brackets for multiple chars.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{cc}}de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("cc", key_map["Digit2"]);

  key_map = rulebased::ParseKeyMapForTesting(L"ab((cc))de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("cc", key_map["Digit2"]);

  // Brackets for empty.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{}}de", false);
  EXPECT_EQ(5UL, key_map.size());
  EXPECT_EQ("a", key_map["BackQuote"]);
  EXPECT_EQ("e", key_map["Digit4"]);
  EXPECT_EQ("", key_map["Digit2"]);

  // Incorrect brackets.
  key_map = rulebased::ParseKeyMapForTesting(L"ab{{cc))de", false);
  EXPECT_EQ(10UL, key_map.size());

  // End with brackets.
  key_map = rulebased::ParseKeyMapForTesting(L"abc{{", false);
  EXPECT_EQ(5UL, key_map.size());
}

}  // namespace ime
}  // namespace chromeos
