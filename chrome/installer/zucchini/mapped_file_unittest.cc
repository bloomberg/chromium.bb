// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/mapped_file.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class MappedFileWriterTest : public testing::Test {
 protected:
  MappedFileWriterTest() = default;
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_path_ = temp_dir_.GetPath().AppendASCII("test-file");
  }

  base::FilePath file_path_;

 private:
  base::ScopedTempDir temp_dir_;
};

TEST_F(MappedFileWriterTest, ErrorCreating) {
  // Create a directory |file_path_|, so |file_writer| fails when it tries to
  // open a file with the same name for write.
  ASSERT_TRUE(base::CreateDirectory(file_path_));
  {
    MappedFileWriter file_writer(file_path_, 10);
    EXPECT_FALSE(file_writer.IsValid());
  }
  EXPECT_TRUE(base::PathExists(file_path_));
}

TEST_F(MappedFileWriterTest, Keep) {
  EXPECT_FALSE(base::PathExists(file_path_));
  {
    MappedFileWriter file_writer(file_path_, 10);
    EXPECT_TRUE(file_writer.Keep());
  }
  EXPECT_TRUE(base::PathExists(file_path_));
}

TEST_F(MappedFileWriterTest, DeleteOnClose) {
  EXPECT_FALSE(base::PathExists(file_path_));
  { MappedFileWriter file_writer(file_path_, 10); }
  EXPECT_FALSE(base::PathExists(file_path_));
}

}  // namespace zucchini
