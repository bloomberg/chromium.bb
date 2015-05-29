// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/process_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromecast {

// Verify that a simple process works as expected.
TEST(ProcessUtilsTest, SimpleProcess) {
  // Create a simple command.
  std::vector<std::string> args;
  args.push_back("echo");
  args.push_back("Hello World");

  // Execute the command and collect the output.
  std::string stdout;
  ASSERT_TRUE(GetAppOutput(args, &stdout));

  // Echo will append a newline to the stdout.
  EXPECT_EQ("Hello World\n", stdout);
}

// Verify that false is returned for an invalid command.
TEST(ProcessUtilsTest, InvalidCommand) {
  // Create a command which is not valid.
  std::vector<std::string> args;
  args.push_back("invalid_command");

  // The command should not run.
  std::string stdout;
  ASSERT_FALSE(GetAppOutput(args, &stdout));
  ASSERT_TRUE(stdout.empty());
}

// Verify that false is returned when a command an error code.
TEST(ProcessUtilsTest, ProcessReturnsError) {
  // Create a simple command.
  std::vector<std::string> args;
  args.push_back("cd");
  args.push_back("path/to/invalid/directory");
  args.push_back("2>&1");  // Pipe the stderr into stdout.

  // Execute the command and collect the output. Verify that the output of the
  // process is collected, even when the process returns an error code.
  std::string stderr;
  ASSERT_FALSE(GetAppOutput(args, &stderr));
  ASSERT_FALSE(stderr.empty());
}

}  // namespace chromecast