// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StatusTest, Ok) {
  Status ok(kOk);
  ASSERT_TRUE(ok.IsOk());
  ASSERT_FALSE(ok.IsError());
  ASSERT_EQ(kOk, ok.code());
  ASSERT_STREQ("ok", ok.message().c_str());
}

TEST(StatusTest, Error) {
  Status ok(kUnknownCommand);
  ASSERT_FALSE(ok.IsOk());
  ASSERT_TRUE(ok.IsError());
  ASSERT_EQ(kUnknownCommand, ok.code());
  ASSERT_STREQ("unknown command", ok.message().c_str());
}

TEST(StatusTest, ErrorWithDetails) {
  Status ok(kUnknownError, "something happened");
  ASSERT_FALSE(ok.IsOk());
  ASSERT_TRUE(ok.IsError());
  ASSERT_EQ(kUnknownError, ok.code());
  ASSERT_STREQ("unknown error: something happened", ok.message().c_str());
}
