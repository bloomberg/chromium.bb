// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/input_method_whitelist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

class InputMethodWhitelistTest : public testing::Test {
 protected:
  const InputMethodWhitelist whitelist_;
};

}  // namespace

TEST_F(InputMethodWhitelistTest, TestInputMethodIdIsWhitelisted) {
  EXPECT_TRUE(whitelist_.InputMethodIdIsWhitelisted("xkb:us:dvorak:eng"));
  EXPECT_FALSE(whitelist_.InputMethodIdIsWhitelisted("mozc,"));
  EXPECT_FALSE(whitelist_.InputMethodIdIsWhitelisted(
      "mozc-pinyin,xkb:us:dvorak:eng"));
  EXPECT_FALSE(whitelist_.InputMethodIdIsWhitelisted("not-supported-id"));
  EXPECT_FALSE(whitelist_.InputMethodIdIsWhitelisted(","));
  EXPECT_FALSE(whitelist_.InputMethodIdIsWhitelisted(""));
}

}  // namespace input_method
}  // namespace chromeos
