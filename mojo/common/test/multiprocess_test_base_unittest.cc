// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/test/multiprocess_test_base.h"

#include "base/logging.h"
// TODO(vtl): Remove build_config.h include when fully implemented on Windows.
#include "build/build_config.h"

namespace mojo {
namespace {

class MultiprocessTestBaseTest : public test::MultiprocessTestBase {
};

TEST_F(MultiprocessTestBaseTest, RunChild) {
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  EXPECT_TRUE(platform_server_channel.get());
  EXPECT_TRUE(platform_server_channel->is_valid());
#endif
  StartChild("RunChild");
  EXPECT_EQ(123, WaitForChildShutdown());
}

MOJO_MULTIPROCESS_TEST_CHILD_MAIN(RunChild) {
// TODO(vtl): Not implemented on Windows yet.
#if defined(OS_POSIX)
  CHECK(MultiprocessTestBaseTest::platform_client_channel.get());
  CHECK(MultiprocessTestBaseTest::platform_client_channel->is_valid());
  // TODO(vtl): Check the client channel.
#endif
  return 123;
}

TEST_F(MultiprocessTestBaseTest, TestChildMainNotFound) {
  StartChild("NoSuchTestChildMain");
  int result = WaitForChildShutdown();
  EXPECT_FALSE(result >= 0 && result <= 127);
}

}  // namespace
}  // namespace mojo
