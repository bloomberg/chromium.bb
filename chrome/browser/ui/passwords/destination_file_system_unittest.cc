// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/destination_file_system.h"

#include <memory>

#include "base/files/file_util.h"
#include "build/build_config.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DestinationFileSystemTest : public testing::Test {
 public:
  DestinationFileSystemTest() = default;
  ~DestinationFileSystemTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(base::CreateTemporaryFile(&temp_file_));
    destination_ = std::make_unique<DestinationFileSystem>(temp_file_);
  }

  void TearDown() override { ASSERT_TRUE(base::DeleteFile(temp_file_, false)); }

 protected:
  base::FilePath temp_file_;
  std::unique_ptr<DestinationFileSystem> destination_;

 private:
  DISALLOW_COPY_AND_ASSIGN(DestinationFileSystemTest);
};

TEST_F(DestinationFileSystemTest, WriteToFile) {
  const std::string kData = "Anything";

  destination_->Write(kData);

  std::string actual_written;
  base::ReadFileToString(temp_file_, &actual_written);
  EXPECT_EQ(kData, actual_written);
}

TEST_F(DestinationFileSystemTest, WriteNothingToFile) {
  const std::string kData;

  destination_->Write(kData);

  std::string actual_written;
  base::ReadFileToString(temp_file_, &actual_written);
  EXPECT_EQ(kData, actual_written);
}

}  // namespace
