// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

class FirstRunTest : public testing::Test {
 protected:
  FirstRunTest() {}
  virtual ~FirstRunTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    PathService::Override(chrome::DIR_USER_DATA, temp_dir_.path());
    first_run::internal::GetFirstRunSentinelFilePath(&sentinel_path_);
  }

  FilePath sentinel_path_;

 private:
  ScopedTempDir temp_dir_;

  DISALLOW_COPY_AND_ASSIGN(FirstRunTest);
};

TEST_F(FirstRunTest, RemoveSentinel) {
  EXPECT_TRUE(first_run::CreateSentinel());
  EXPECT_TRUE(file_util::PathExists(sentinel_path_));

  EXPECT_TRUE(first_run::RemoveSentinel());
  EXPECT_FALSE(file_util::PathExists(sentinel_path_));
}
