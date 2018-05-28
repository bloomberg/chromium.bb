// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <locale>

#include "components/autofill/core/browser/password_generator_fips181.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckPasswordCorrectness(const std::string& password) {
  int num_upper_case_letters = 0;
  int num_lower_case_letters = 0;
  int num_digits = 0;
  for (size_t i = 0; i < password.size(); i++) {
    if (isupper(password[i]))
      ++num_upper_case_letters;
    else if (islower(password[i]))
      ++num_lower_case_letters;
    else if (isdigit(password[i]))
      ++num_digits;
  }
  EXPECT_GT(num_upper_case_letters, 0) << password;
  EXPECT_GT(num_lower_case_letters, 0) << password;
  EXPECT_GT(num_digits, 0) << password;
}

}  // namespace

namespace autofill {

TEST(PasswordGeneratorFips181Test, PasswordLength) {
  PasswordGeneratorFips181 pg1(10);
  std::string password = pg1.Generate();
  EXPECT_EQ(password.size(), 10u);

  PasswordGeneratorFips181 pg2(-1);
  password = pg2.Generate();
  EXPECT_EQ(
      password.size(),
      static_cast<size_t>(PasswordGeneratorFips181::kDefaultPasswordLength));

  PasswordGeneratorFips181 pg3(100);
  password = pg3.Generate();
  EXPECT_EQ(
      password.size(),
      static_cast<size_t>(PasswordGeneratorFips181::kDefaultPasswordLength));
}

TEST(PasswordGeneratorFips181Test, PasswordPattern) {
  PasswordGeneratorFips181 pg(12);
  std::string password = pg.Generate();
  CheckPasswordCorrectness(password);
}

TEST(PasswordGeneratorFips181Test, Printable) {
  PasswordGeneratorFips181 pg(12);
  std::string password = pg.Generate();
  for (size_t i = 0; i < password.size(); i++) {
    // Make sure that the character is printable.
    EXPECT_TRUE(isgraph(password[i]));
  }
}

TEST(PasswordGeneratorFips181Test, ForceFixPasswordTest) {
  std::string passwords_to_fix[] = {"nonumbersoruppercase",
                                    "nonumbersWithuppercase",
                                    "numbers3Anduppercase", "UmpAwgemHoc"};
  for (auto& password : passwords_to_fix) {
    ForceFixPassword(&password);
    CheckPasswordCorrectness(password);
  }
}

}  // namespace autofill
