// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "courgette/base_test_unittest.h"

#include "base/path_service.h"

void BaseTest::SetUp() {
  PathService::Get(base::DIR_SOURCE_ROOT, &test_dir_);
  test_dir_ = test_dir_.AppendASCII("courgette");
  test_dir_ = test_dir_.AppendASCII("testdata");
}

void BaseTest::TearDown() {
}

//  Reads a test file into a string.
std::string BaseTest::FileContents(const char* file_name) const {
  FilePath file_path = test_dir_;
  file_path = file_path.AppendASCII(file_name);
  std::string file_bytes;

  EXPECT_TRUE(file_util::ReadFileToString(file_path, &file_bytes));

  return file_bytes;
}
