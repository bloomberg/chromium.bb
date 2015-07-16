// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/files/file_util.h"
#include "base/rand_util.h"
#include "chromecast/crash/linux/dummy_minidump_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

TEST(DummyMinidumpGeneratorTest, GenerateFailsWithInvalidPath) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Attempt to generate a minidump from an invalid path.
  DummyMinidumpGenerator generator("/path/does/not/exist/minidump.dmp");
  ASSERT_FALSE(generator.Generate(minidump_dir.Append("minidump.dmp").value()));
}

TEST(DummyMinidumpGeneratorTest, GenerateSucceedsWithSmallSource) {
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

TEST(DummyMinidumpGeneratorTest, GenerateSucceedsWithLargeSource) {
  // Create directory in which to put minidump.
  base::FilePath minidump_dir;
  ASSERT_TRUE(base::CreateNewTempDirectory("", &minidump_dir));

  // Create a large fake minidump file.
  base::FilePath fake_minidump;
  ASSERT_TRUE(base::CreateTemporaryFile(&fake_minidump));
  size_t str_len = 32768 * 10 + 1;
  const std::string data = base::RandBytesAsString(str_len);

  // Write the string to the file.
  ASSERT_EQ(static_cast<int>(data.size()),
            base::WriteFile(fake_minidump, data.c_str(), data.size()));

  base::FilePath new_minidump = minidump_dir.Append("minidump.dmp");
  DummyMinidumpGenerator generator(fake_minidump.value());
  ASSERT_TRUE(generator.Generate(new_minidump.value()));

  // Original file should not exist, and new file should contain original
  // contents.
  std::string copied_data;
  EXPECT_FALSE(base::PathExists(fake_minidump));
  ASSERT_TRUE(base::PathExists(new_minidump));
  EXPECT_TRUE(base::ReadFileToString(new_minidump, &copied_data));
  EXPECT_EQ(data, copied_data);
}

}  // namespace chromecast
