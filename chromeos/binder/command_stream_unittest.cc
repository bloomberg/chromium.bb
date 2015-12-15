// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>
#include <linux/android/binder.h>

#include "chromeos/binder/command_stream.h"
#include "chromeos/binder/driver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace binder {

namespace {

// TODO(hashimoto): Add tests for Fetch() and ProcessCommand().
class BinderCommandStreamTest : public ::testing::Test,
                                public CommandStream::IncomingCommandHandler {
 public:
  BinderCommandStreamTest() : command_stream_(&driver_, this) {}
  ~BinderCommandStreamTest() override {}

  void SetUp() override { ASSERT_TRUE(driver_.Initialize()); }

 protected:
  Driver driver_;
  CommandStream command_stream_;
};

}  // namespace

TEST_F(BinderCommandStreamTest, EnterLooper) {
  command_stream_.AppendOutgoingCommand(BC_ENTER_LOOPER, nullptr, 0);
  EXPECT_TRUE(command_stream_.Flush());
  command_stream_.AppendOutgoingCommand(BC_EXIT_LOOPER, nullptr, 0);
  EXPECT_TRUE(command_stream_.Flush());
}

TEST_F(BinderCommandStreamTest, Error) {
  // Kernel's binder.h says BC_ATTEMPT_ACQUIRE is not currently supported.
  binder_pri_desc params = {};
  command_stream_.AppendOutgoingCommand(BC_ATTEMPT_ACQUIRE, &params,
                                        sizeof(params));
  EXPECT_FALSE(command_stream_.Flush());
}

}  // namespace binder
