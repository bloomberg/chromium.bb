// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/ui/auto_login_prompter.h"
#include "testing/gtest/include/gtest/gtest.h"

class AutoLoginPrompterTest : public testing::Test {
 protected:
  typedef AutoLoginPrompter::Params Params;

  static bool ParseAutoLoginHeader(const std::string& input, Params* output) {
    return AutoLoginPrompter::ParseAutoLoginHeader(input, output);
  }

  static bool IsParamsEmpty(const Params& params) {
    return params.realm.empty() && params.account.empty() &&
        params.args.empty() && params.username.empty();
  }
};

TEST_F(AutoLoginPrompterTest, ParseAutoLoginHeader) {
  std::string header =
      "realm=com.google&"
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  Params params;
  EXPECT_TRUE(ParseAutoLoginHeader(header, &params));

  ASSERT_EQ("com.google", params.realm);
  ASSERT_EQ("fred.example@gmail.com", params.account);
  ASSERT_EQ("kfdshfwoeriudslkfsdjfhdskjfhsdkr", params.args);
}

TEST_F(AutoLoginPrompterTest, ParseAutoLoginHeaderOnlySupportsComGoogle) {
  std::string header =
      "realm=com.microsoft&"
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  Params params;
  EXPECT_FALSE(ParseAutoLoginHeader(header, &params));
  // |params| should not be touched when parsing fails.
  EXPECT_TRUE(IsParamsEmpty(params));
}

TEST_F(AutoLoginPrompterTest, ParseAutoLoginHeaderWithMissingRealm) {
  std::string header =
      "account=fred.example%40gmail.com&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  Params params;
  EXPECT_FALSE(ParseAutoLoginHeader(header, &params));
  EXPECT_TRUE(IsParamsEmpty(params));
}

TEST_F(AutoLoginPrompterTest, ParseAutoLoginHeaderWithMissingArgs) {
  std::string header =
      "realm=com.google&"
      "account=fred.example%40gmail.com&";

  Params params;
  EXPECT_FALSE(ParseAutoLoginHeader(header, &params));
  EXPECT_TRUE(IsParamsEmpty(params));
}

TEST_F(AutoLoginPrompterTest, ParseAutoLoginHeaderWithoutOptionalAccount) {
  std::string header =
      "realm=com.google&"
      "args=kfdshfwoeriudslkfsdjfhdskjfhsdkr";

  Params params;
  EXPECT_TRUE(ParseAutoLoginHeader(header, &params));
  ASSERT_EQ("com.google", params.realm);
  ASSERT_EQ("kfdshfwoeriudslkfsdjfhdskjfhsdkr", params.args);
}
