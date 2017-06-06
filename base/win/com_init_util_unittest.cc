// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/com_init_util.h"

#include "base/test/gtest_util.h"
#include "base/win/scoped_com_initializer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

TEST(ComInitUtil, AssertNotInitialized) {
  EXPECT_DCHECK_DEATH(AssertComInitialized());
}

TEST(ComInitUtil, AssertUninitialized) {
  {
    ScopedCOMInitializer com_initializer;
    ASSERT_TRUE(com_initializer.succeeded());
  }
  EXPECT_DCHECK_DEATH(AssertComInitialized());
}

TEST(ComInitUtil, AssertSTAInitialized) {
  ScopedCOMInitializer com_initializer;
  ASSERT_TRUE(com_initializer.succeeded());

  AssertComInitialized();
}

TEST(ComInitUtil, AssertMTAInitialized) {
  ScopedCOMInitializer com_initializer(ScopedCOMInitializer::kMTA);
  ASSERT_TRUE(com_initializer.succeeded());

  AssertComInitialized();
}

}  // namespace win
}  // namespace base
