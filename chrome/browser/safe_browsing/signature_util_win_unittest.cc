// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SignatureUtilWinTest, IsSigned) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_path));

  FilePath testdata_path = source_path
      .AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("safe_browsing")
      .AppendASCII("download_protection");

  EXPECT_TRUE(signature_util::IsSigned(testdata_path.Append(L"signed.exe")));
  EXPECT_FALSE(signature_util::IsSigned(
      testdata_path.Append(L"unsigned.exe")));
  EXPECT_FALSE(signature_util::IsSigned(
      testdata_path.Append(L"doesnotexist.exe")));
}

}  // namespace safe_browsing
