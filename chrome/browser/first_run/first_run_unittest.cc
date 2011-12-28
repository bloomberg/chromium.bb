// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirstRunTest : public testing::Test {
 protected:
  virtual void SetUp() {
    first_run::internal::GetFirstRunSentinelFilePath(&sentinel_path_);
  }

  FilePath sentinel_path_;
};

TEST_F(FirstRunTest, RemoveSentinel) {
  EXPECT_TRUE(first_run::CreateSentinel());
  EXPECT_TRUE(file_util::PathExists(sentinel_path_));

  EXPECT_TRUE(first_run::RemoveSentinel());
  EXPECT_FALSE(file_util::PathExists(sentinel_path_));
}
