// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chromeos/ime/input_method_descriptor.h"
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
  InputMethodDescriptor GetDescById(const std::string& id) {
    return InputMethodDescriptor(id,
                                 "",  // name
                                 "us",
                                 "language_code",
                                 false);
  }
};

}  // namespace

TEST_F(InputMethodDescriptorTest, TestOperatorEqual) {
  EXPECT_EQ(GetDescById("xkb:us::eng"), GetDescById("xkb:us::eng"));
  EXPECT_NE(GetDescById("xkb:us::eng"), GetDescById("xkb:us:dvorak:eng"));
  EXPECT_NE(GetDescById("xkb:fr::fra"), GetDescById("xkb:us::eng"));
}

}  // namespace input_method
}  // namespace chromeos
