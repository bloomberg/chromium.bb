// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/zip_analyzer.h"

#include "base/path_service.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {
namespace zip_analyzer {

TEST(CountLocalFileHeaders, CountsOneBinary) {
  base::FilePath test_file_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_path));
  test_file_path = test_file_path.AppendASCII(
      "safe_browsing/download_protection/zipfile_one_unsigned_binary.zip");

  base::File test_file(test_file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_EQ(1, CountLocalFileHeaders(&test_file));
}

TEST(CountLocalFileHeaders, CountsTwoBinaries) {
  base::FilePath test_file_path;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_file_path));
  test_file_path = test_file_path.AppendASCII(
      "safe_browsing/download_protection/zipfile_two_binaries_one_signed.zip");

  base::File test_file(test_file_path,
                       base::File::FLAG_OPEN | base::File::FLAG_READ);
  ASSERT_EQ(2, CountLocalFileHeaders(&test_file));
}

}  // namespace zip_analyzer
}  // namespace safe_browsing
