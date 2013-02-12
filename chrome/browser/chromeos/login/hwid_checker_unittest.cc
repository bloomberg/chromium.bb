// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/hwid_checker.h"

#include "testing/gtest/include/gtest/gtest.h"


namespace chromeos {

TEST(HWIDCheckerTest, EmptyHWID) {
  EXPECT_FALSE(IsHWIDCorrect(""));
}

TEST(HWIDCheckerTest, MalformedChecksum) {
  EXPECT_TRUE(IsHWIDCorrect("SOME DATA 7861"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 7861 "));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786 1"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA 786"));
  EXPECT_FALSE(IsHWIDCorrect("SOME DATA7861"));
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

