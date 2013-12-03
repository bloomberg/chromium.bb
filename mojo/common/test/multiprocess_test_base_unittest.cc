// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

namespace mojo {
namespace {

class MultiprocessTestBaseTest : public test::MultiprocessTestBase {
};

TEST_F(MultiprocessTestBaseTest, RunChild) {
  StartChild("RunChild");
  EXPECT_EQ(123, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(RunChild) {
  return 123;
}

TEST_F(MultiprocessTestBaseTest, TestChildMainNotFound) {
  StartChild("NoSuchTestChildMain");
  int result = WaitForChildShutdown();
  EXPECT_FALSE(result >= 0 && result <= 127);
}

}  // namespace
}  // namespace mojo
