// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include "base/file_path.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "chrome/installer/mini_installer/decompress.h"
#include "testing/gtest/include/gtest/gtest.h"

class MiniDecompressTest : public testing::Test {
 protected:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

TEST_F(MiniDecompressTest, ExpandTest) {
  FilePath source_path;
  PathService::Get(base::DIR_SOURCE_ROOT, &source_path);
  source_path = source_path.Append(FILE_PATH_LITERAL("chrome"))
      .Append(FILE_PATH_LITERAL("installer"))
      .Append(FILE_PATH_LITERAL("test"))
      .Append(FILE_PATH_LITERAL("data"))
      .Append(FILE_PATH_LITERAL("SETUP.EX_"));

  // Prepare a temp folder that will be automatically deleted along with
  // our temporary test data.
  ScopedTempDir temp_dir;
  EXPECT_TRUE(temp_dir.CreateUniqueTempDir());
  FilePath dest_path(temp_dir.path().Append(FILE_PATH_LITERAL("setup.exe")));

  // Decompress our test file.
  EXPECT_TRUE(mini_installer::Expand(source_path.value().c_str(),
                                     dest_path.value().c_str()));

  // Check if the expanded file is a valid executable.
  DWORD type = static_cast<DWORD>(-1);
  EXPECT_TRUE(GetBinaryType(dest_path.value().c_str(), &type));
  EXPECT_EQ(SCS_32BIT_BINARY, type);
}
