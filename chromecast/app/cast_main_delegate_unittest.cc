// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/memory/ptr_util.h"
#include "chromecast/app/cast_main_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {
namespace shell {
namespace {

const char kTestCommandLineContents[] = "--file1\n--file2\n";
const char kTestCommandLinePath[] = "/tmp/test-command-line";

void WriteTestCommandLine() {
  base::File command_line(
      base::FilePath(kTestCommandLinePath),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_GE(command_line.Write(0, kTestCommandLineContents,
                               sizeof(kTestCommandLineContents) - 1),
            0);
}

}  // namespace

class CastMainDelegateTest : public testing::Test {
 public:
  CastMainDelegateTest() { WriteTestCommandLine(); }

  ~CastMainDelegateTest() override {}

  // testing::Test implementation:
  void SetUp() override {
    test_argv_strs_.push_back("--main1");
    test_argv_strs_.push_back("--main2");
    test_argv_.push_back(test_argv_strs_[0].c_str());
    test_argv_.push_back(test_argv_strs_[1].c_str());
  }

  void CreateCastMainDelegate(int argc,
                              const char** argv,
                              std::string command_line_path) {
    delegate_ = base::WrapUnique(
        new CastMainDelegate(argc, argv, base::FilePath(command_line_path)));
  }

 protected:
  std::unique_ptr<CastMainDelegate> delegate_;
  std::vector<std::string> test_argv_strs_;
  std::vector<const char*> test_argv_;
};

TEST_F(CastMainDelegateTest, AddsArgsFromFile) {
  CreateCastMainDelegate(0, nullptr, kTestCommandLinePath);
  EXPECT_EQ(2, delegate_->argc());
  EXPECT_EQ("--file1", std::string(delegate_->argv()[0]));
  EXPECT_EQ("--file2", std::string(delegate_->argv()[1]));
}

TEST_F(CastMainDelegateTest, AddsArgsFromMain) {
  CreateCastMainDelegate(test_argv_.size(), &test_argv_[0], "");
  EXPECT_EQ(2, delegate_->argc());
  EXPECT_EQ("--main1", std::string(delegate_->argv()[0]));
  EXPECT_EQ("--main2", std::string(delegate_->argv()[1]));
}

TEST_F(CastMainDelegateTest, MergesArgsFromFileAndMain) {
  CreateCastMainDelegate(test_argv_.size(), &test_argv_[0],
                         kTestCommandLinePath);
  EXPECT_EQ(4, delegate_->argc());
  EXPECT_EQ("--main1", std::string(delegate_->argv()[0]));
  EXPECT_EQ("--main2", std::string(delegate_->argv()[1]));
  EXPECT_EQ("--file1", std::string(delegate_->argv()[2]));
  EXPECT_EQ("--file2", std::string(delegate_->argv()[3]));
}

}  // namespace shell
}  // namespace chromecast
