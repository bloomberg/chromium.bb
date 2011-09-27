// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/mac/mac_util.h"
#include "content/browser/mac/closure_blocks_leopard_compat.h"

// Used in ClosureBlocksLeopardCompatTest.Global. Putting a block at global
// scope results in a block structure whose isa field is set to
// NSConcreteGlobalBlock, ensuring that symbol is referenced. This test needs
// to be able to load on 10.5 where that symbol is not present at runtime, so
// the real test here is that the symbol is weakly imported. If not, the
// executable will fail to load.
void (^global_block)(bool*) = ^(bool* ran_pointer) { *ran_pointer = true; };

namespace {

// It should be possible to declare a block on OSes as early as Leopard
// (10.5). This should be buildable with the 10.5 SDK and a 10.5 deployment
// target, and runnable on 10.5. However, the block itself is not expected to
// be runnable on 10.5. This test verfies that blocks are buildable on 10.5
// and runnable on Snow Leopard (10.6) and later.

TEST(ClosureBlocksLeopardCompatTest, Stack) {
  bool ran = false;

  // This should result in a block structure whose isa field is set to
  // NSConcreteStackBlock, ensuring that symbol is referenced. This test
  // needs to be able to load on 10.5 where that symbol is not present at
  // runtime, so the real test here checks that the symbol is weakly imported.
  // If not, the executable will fail to load.

  void (^test_block)(bool*) = ^(bool* ran_pointer) { *ran_pointer = true; };

  ASSERT_FALSE(ran);

  if (base::mac::IsOSSnowLeopardOrLater()) {
    test_block(&ran);

    EXPECT_TRUE(ran);
  }
}

TEST(ClosureBlocksLeopardCompatTest, Global) {
  bool ran = false;

  ASSERT_FALSE(ran);

  if (base::mac::IsOSSnowLeopardOrLater()) {
    global_block(&ran);

    EXPECT_TRUE(ran);
  }
}

TEST(ClosureBlocksLeopardCompatTest, CopyRelease) {
  if (base::mac::IsOSSnowLeopardOrLater()) {
    // Protect the entire test in this conditional, because __block won't run
    // on 10.5.
    __block int value = 0;

    typedef int (^TestBlockType)();
    TestBlockType local_block = ^(void) { return ++value; };

    ASSERT_EQ(0, value);

    int rv = local_block();
    EXPECT_EQ(1, rv);
    EXPECT_EQ(rv, value);

    // Using Block_copy, Block_release, and these other operations ensures
    // that _Block_copy, _Block_release, _Block_object_assign, and
    // _Block_object_dispose are referenced. This test needs to be able to
    // load on 10.5 where these symbols are not present at runtime, so the
    // real test here is that the symbol is weakly imported. If not, the
    // executable will fail to load.

    TestBlockType copied_block = Block_copy(local_block);

    rv = copied_block();
    EXPECT_EQ(2, rv);
    EXPECT_EQ(rv, value);

    rv = copied_block();
    EXPECT_EQ(3, rv);
    EXPECT_EQ(rv, value);

    Block_release(copied_block);

    rv = local_block();
    EXPECT_EQ(4, rv);
    EXPECT_EQ(rv, value);
  }
}

}  // namespace
