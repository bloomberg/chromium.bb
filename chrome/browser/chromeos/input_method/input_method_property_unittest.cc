// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/chromeos/input_method/input_method_property.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(InputMethodPropertyTest, TestOperatorEqual) {
  InputMethodProperty empty;
  InputMethodProperty reference("key", "label", true, true, 1);

  InputMethodProperty p1("X", "label", true, true, 1);
  InputMethodProperty p2("key", "X", true, true, 1);
  InputMethodProperty p3("key", "label", false, true, 1);
  InputMethodProperty p4("key", "label", true, false, 1);
  InputMethodProperty p5("key", "label", true, true, 0);

  EXPECT_EQ(empty, empty);
  EXPECT_EQ(reference, reference);
  EXPECT_NE(reference, empty);
  EXPECT_NE(reference, p1);
  EXPECT_NE(reference, p2);
  EXPECT_NE(reference, p3);
  EXPECT_NE(reference, p4);
  EXPECT_NE(reference, p5);
}

}  // namespace input_method
}  // namespace chromeos
