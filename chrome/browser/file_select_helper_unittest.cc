// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/file_select_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FileSelectHelperTest, IsAcceptTypeValid) {
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("a/b"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/def"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid("abc/*"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".a"));
  EXPECT_TRUE(FileSelectHelper::IsAcceptTypeValid(".abc"));

  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("."));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("/"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("ABC/*"));
  EXPECT_FALSE(FileSelectHelper::IsAcceptTypeValid("abc/def "));
}
