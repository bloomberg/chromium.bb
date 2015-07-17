// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

namespace {
// This value should stay in sync with the internal buffer size used in
// CopyAndDelete().
const int kInternalBufferSize = 32768;
}  // namespace

TEST(DummyMinidumpGeneratorTest, GenerateFailsWithInvalidPath) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Attempt to generate a minidump from an invalid path.
  DummyMinidumpGenerator generator("/path/does/not/exist/minidump.dmp");
  ASSERT_FALSE(generator.Generate(minidump_dir.Append("minidump.dmp").value()));
}

TEST(DummyMinidumpGeneratorTest, GenerateSucceedsWithValidPath) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Create a fake minidump file.
  base::FilePath fake_minidump;
  ASSERT_TRUE(base::CreateTemporaryFile(&fake_minidump));
  const std::string data("Test contents of the minidump file.\n");
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(fake_minidump, data.c_str(), data.size()));

  DummyMinidumpGenerator generator(fake_minidump.value());
  base::FilePath new_minidump = minidump_dir.Append("minidump.dmp");
  EXPECT_TRUE(generator.Generate(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(DummyMinidumpGeneratorTest, CopyAndDeleteFailsWithInvalidSource) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Attempt to copy from an invalid path.
  DummyMinidumpGenerator generator("/path/does/not/exist/minidump.dmp");
  ASSERT_FALSE(generator.CopyAndDeleteForTest(
      minidump_dir.Append("minidump.dmp").value()));
}

TEST(DummyMinidumpGeneratorTest, CopyAndDeleteSucceedsWithSmallSource) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Create a fake minidump file.
  base::FilePath fake_minidump;
  ASSERT_TRUE(base::CreateTemporaryFile(&fake_minidump));
  const std::string data("Test contents of the minidump file.\n");
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(fake_minidump, data.c_str(), data.size()));

  base::FilePath new_minidump = minidump_dir.Append("minidump.dmp");
  DummyMinidumpGenerator generator(fake_minidump.value());
  ASSERT_TRUE(generator.CopyAndDeleteForTest(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(DummyMinidumpGeneratorTest, CopyAndDeleteSucceedsWithLargeSource) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Create a large fake minidump file.
  // Note: The file must be greater than the size of the buffer used to copy the
  // file in CopyAndDelete(). Create a big string in memory.
  base::FilePath fake_minidump;
  ASSERT_TRUE(base::CreateTemporaryFile(&fake_minidump));
  size_t str_len = kInternalBufferSize * 10 + 1;
  const std::string data = base::RandBytesAsString(str_len);

  // Write the string to the file and verify that the file is big enough.
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(fake_minidump, data.c_str(), data.size()));
  int64_t filesize = 0;
  ASSERT_TRUE(base::GetFileSize(fake_minidump, &filesize));
  ASSERT_GT(filesize, kInternalBufferSize);

  base::FilePath new_minidump = minidump_dir.Append("minidump.dmp");
  DummyMinidumpGenerator generator(fake_minidump.value());
  ASSERT_TRUE(generator.CopyAndDeleteForTest(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

TEST(DummyMinidumpGeneratorTest, CopyAndDeleteSucceedsWhenEOFAlignsWithBuffer) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Create a large fake minidump file.
  // Note: The file must be greater than the size of the buffer used to copy the
  // file in CopyAndDelete(). Create a big string in memory.
  base::FilePath fake_minidump;
  ASSERT_TRUE(base::CreateTemporaryFile(&fake_minidump));
  size_t str_len = kInternalBufferSize;
  const std::string data = base::RandBytesAsString(str_len);

  // Write the string to the file and verify that the file is big enough.
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(fake_minidump, data.c_str(), data.size()));
  int64_t filesize = 0;
  ASSERT_TRUE(base::GetFileSize(fake_minidump, &filesize));
  ASSERT_EQ(kInternalBufferSize, filesize);

  base::FilePath new_minidump = minidump_dir.Append("minidump.dmp");
  DummyMinidumpGenerator generator(fake_minidump.value());
  ASSERT_TRUE(generator.CopyAndDeleteForTest(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

}  // namespace chromecast
