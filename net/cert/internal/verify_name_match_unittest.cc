// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/internal/verify_name_match.h"

#include "net/der/input.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(VerifyNameMatchTest, Simple) {
  // TODO(mattm): Use valid Names.
  EXPECT_TRUE(VerifyNameMatch(der::Input("hello"), der::Input("hello")));
  EXPECT_FALSE(VerifyNameMatch(der::Input("aello"), der::Input("hello")));
  EXPECT_FALSE(VerifyNameMatch(der::Input("hello"), der::Input("hello1")));
  EXPECT_FALSE(VerifyNameMatch(der::Input("hello1"), der::Input("hello")));
}

}  // namespace net
