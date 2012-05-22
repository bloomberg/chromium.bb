// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/input_method_descriptor.h"
#include "chrome/browser/chromeos/input_method/input_method_whitelist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

namespace {

const char kFallbackLayout[] = "us";

class InputMethodDescriptorTest : public testing::Test {
 public:
  InputMethodDescriptorTest() {
  }

 protected:
  InputMethodDescriptor GetDesc(const std::string& raw_layout) {
    return InputMethodDescriptor(whitelist_,
                                 "id",
                                 "",  // name
                                 raw_layout,
                                 "language_code");
  }

  InputMethodDescriptor GetDescById(const std::string& id) {
    return InputMethodDescriptor(whitelist_,
                                 id,
                                 "",  // name
                                 "us",
                                 "language_code");
  }

 private:
  const InputMethodWhitelist whitelist_;
};

}  // namespace

TEST_F(InputMethodDescriptorTest, TestCreateInputMethodDescriptor) {
  EXPECT_EQ("us", GetDesc("us").keyboard_layout());
  EXPECT_EQ("us", GetDesc("us,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)", GetDesc("us(dvorak),us").keyboard_layout());

  EXPECT_EQ("fr", GetDesc("fr").keyboard_layout());
  EXPECT_EQ("fr", GetDesc("fr,us(dvorak)").keyboard_layout());
  EXPECT_EQ("us(dvorak)", GetDesc("us(dvorak),fr").keyboard_layout());

  EXPECT_EQ("fr", GetDesc("not-supported,fr").keyboard_layout());
  EXPECT_EQ("fr", GetDesc("fr,not-supported").keyboard_layout());

  EXPECT_EQ(kFallbackLayout, GetDesc("not-supported").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc(",").keyboard_layout());
  EXPECT_EQ(kFallbackLayout, GetDesc("").keyboard_layout());
}

TEST_F(InputMethodDescriptorTest, TestOperatorEqual) {
  EXPECT_EQ(GetDescById("xkb:us::eng"), GetDescById("xkb:us::eng"));
  EXPECT_NE(GetDescById("xkb:us::eng"), GetDescById("xkb:us:dvorak:eng"));
  EXPECT_NE(GetDescById("xkb:fr::fra"), GetDescById("xkb:us::eng"));
}

}  // namespace input_method
}  // namespace chromeos
