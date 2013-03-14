// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/hwid_checker.h"

#include "testing/gtest/include/gtest/gtest.h"


namespace chromeos {

TEST(HWIDCheckerTest, EmptyHWID) {
  EXPECT_FALSE(IsHWIDCorrect(""));
}

TEST(HWIDCheckerTest, HWIDv2) {
  EXPECT_TRUE(IsHWIDCorrect("SOME DATA 7861"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 7861 "));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786 1"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA7861"));
}

TEST(HWIDCheckerTest, HWIDv3) {
  EXPECT_TRUE(IsHWIDCorrect("SPRING 3A7N-BJKZ-F"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING 3A7N-BJKK-HB"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING 3A7N-BJKK-EHU"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING 3A7N-BJKK-MGG"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING 3A7N-BJKK-M4RF"));

  // Degenerate cases.
  EXPECT_FALSE(IsHWIDCorrect("SPRING"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING "));
  EXPECT_FALSE(IsHWIDCorrect("SPRING KD"));
  EXPECT_TRUE(IsHWIDCorrect("SPRING T7"));

  // No board name.
  EXPECT_FALSE(IsHWIDCorrect(" 3A7N-BJKU-Y"));
  EXPECT_FALSE(IsHWIDCorrect("3A7N-BJKG-Z"));

  // Excess fields.
  EXPECT_FALSE(IsHWIDCorrect("SPRING WINTER 3A7N-BJK6-Y"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-BJKX-G WINTER"));

  // Incorrect BOM format.
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7-NBJK-ZF"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A-7NBJ-KZF"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING -3A7N-BJKZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-BJKK-FSYV-"));

  // Incorrect characters.
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A9N-BJKK-E"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-B0KS-Z"));
  EXPECT_FALSE(IsHWIDCorrect("SPrING 3A7N-BJKG-5"));

  // Random changes.
  EXPECT_FALSE(IsHWIDCorrect("SPRUNG 3A7N-BJKZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-8JKZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-BJSZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRINGS 3A7N-BJKZ-F"));
  EXPECT_FALSE(IsHWIDCorrect("SPRING 3A7N-BJKK-L"));
  EXPECT_FALSE(IsHWIDCorrect("SPRINGX3A7N-BJKZ-F"));
}

TEST(HWIDCheckerTest, KnownHWIDs) {
  EXPECT_TRUE(IsHWIDCorrect("DELL HORIZON MAGENTA 8992"));
  EXPECT_FALSE(IsHWIDCorrect("DELL HORIZ0N MAGENTA 8992"));

  EXPECT_TRUE(IsHWIDCorrect("DELL HORIZON MAGENTA DVT 4770"));
  EXPECT_FALSE(IsHWIDCorrect("DELL MAGENTA HORIZON DVT 4770"));

  EXPECT_TRUE(IsHWIDCorrect("SAMS ALEX GAMMA DVT 9247"));
  EXPECT_FALSE(IsHWIDCorrect("SAMS ALPX GAMMA DVT 9247"));
}

} // namespace chromeos

