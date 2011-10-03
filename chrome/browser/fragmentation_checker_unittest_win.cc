// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/path_service.h"
#include "chrome/browser/fragmentation_checker_win.h"
#include "chrome/common/guid.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FragmentationChecker, BasicCheck) {
  FilePath module_path;
  ASSERT_TRUE(PathService::Get(base::FILE_MODULE, &module_path));
  int extent_count = fragmentation_checker::CountFileExtents(module_path);
  EXPECT_GT(extent_count, 0);
}

TEST(FragmentationChecker, InvalidFile) {
  FilePath module_path;
  ASSERT_TRUE(PathService::Get(base::FILE_MODULE, &module_path));
  module_path = module_path.DirName().AppendASCII(guid::GenerateGUID());
  int extent_count = fragmentation_checker::CountFileExtents(module_path);
  EXPECT_EQ(extent_count, 0);
}
