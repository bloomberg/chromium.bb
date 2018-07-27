// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/pre_fetched_paths.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

namespace {

TEST(PreFetchedPathsTest, PathsArePreFetched) {
  PreFetchedPaths* pre_fetched_paths = PreFetchedPaths::GetInstance();
  // Note: pre_fetched_paths->Initialized() is already called in test_main.cc.

  EXPECT_FALSE(pre_fetched_paths->GetExecutablePath().empty());
  EXPECT_FALSE(pre_fetched_paths->GetProgramFilesFolder().empty());
}

}  // namespace

}  // namespace chrome_cleaner
