// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <iostream>
#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Path to the test app used to locate sample app.
std::string s_test_app_path;

// Returns directory name with trailing separator extracted from the file path.
std::string DirName(const std::string& file_path) {
  size_t pos = file_path.find_last_of("\\/");
  if (std::string::npos == pos)
    return std::string();
  return file_path.substr(0, pos + 1);
}

// Runs |command_line| and returns string representation of its stdout.
std::string RunCommand(std::string command_line) {
  FILE* pipe = popen(command_line.c_str(), "r");
  EXPECT_TRUE(pipe) << "popen() failed";
  std::string result;
  while (!feof(pipe)) {
    char buffer[128];
    if (fgets(buffer, sizeof(buffer), pipe))
      result += buffer;
  }
  pclose(pipe);
  return result;
}

// Test that cronet_sample runs and gets connection refused from localhost.
TEST(SampleTest, TestConnectionRefused) {
  // Expect "cronet_sample" app to be located in same directory as the test.
  std::string cronet_sample_path = DirName(s_test_app_path) + "cronet_sample";
  std::string url = "http://localhost:99999";
  std::string sample_out = RunCommand(cronet_sample_path + " " + url);

  // Expect cronet sample to run and fail with net::ERR_INVALID_URL.
  EXPECT_NE(std::string::npos, sample_out.find("net::ERR_INVALID_URL"));
}

}  // namespace

int main(int argc, char** argv) {
  s_test_app_path = argv[0];
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
