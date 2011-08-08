// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/mac/mac_util.h"
#include "chrome/browser/mac/closure_blocks_leopard_compat.h"

namespace {

// It should be possible to declare a block on OSes as early as Leopard
// (10.5). This should be buildable with the 10.5 SDK and a 10.5 deployment
// target, and runnable on 10.5. However, the block itself is not expected to
// be runnable on 10.5. This test verfies that blocks are buildable on 10.5
// and runnable on Snow Leopard (10.6) and later.
TEST(ClosureBlocksLeopardCompatTest, Basic) {
  bool ran = false;

  void (^test_block)(bool*) = ^(bool* ran_pointer) { *ran_pointer = true; };

  ASSERT_FALSE(ran);

  if (base::mac::IsOSSnowLeopardOrLater()) {
    test_block(&ran);

    ASSERT_TRUE(ran);
  }
}

}  // namespace
