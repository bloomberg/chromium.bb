// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <locale>

#include "chrome/browser/autofill/password_generator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(PasswordGeneratorTest, PasswordGeneratorSimpleTest) {
  // Not much to test, just make sure that the characters in a generated
  // password are reasonable.
  PasswordGenerator pg;
  std::string password = pg.Generate();
  for (size_t i = 0; i < password.size(); i++) {
    // Make sure that the character is printable.
    EXPECT_TRUE(isgraph(password[i]));
  }
}

}  // namespace autofill
