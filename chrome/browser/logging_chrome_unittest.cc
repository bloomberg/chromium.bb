// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/env_vars.h"
#include "chrome/common/logging_chrome.h"
#include "testing/gtest/include/gtest/gtest.h"

class ChromeLoggingTest : public testing::Test {
 public:
  // Stores the current value of the log file name environment
  // variable and sets the variable to new_value.
  void SaveEnvironmentVariable(std::string new_value) {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    if (!env->GetVar(env_vars::kLogFileName, &environment_filename_))
      environment_filename_ = "";

    env->SetVar(env_vars::kLogFileName, new_value);
  }

  // Restores the value of the log file nave environment variable
  // previously saved by SaveEnvironmentVariable().
  void RestoreEnvironmentVariable() {
    scoped_ptr<base::Environment> env(base::Environment::Create());
    env->SetVar(env_vars::kLogFileName, environment_filename_);
  }

 private:
  std::string environment_filename_;  // Saves real environment value.
};

// Tests the log file name getter without an environment variable.
TEST_F(ChromeLoggingTest, LogFileName) {
  SaveEnvironmentVariable(std::string());

  base::FilePath filename = logging::GetLogFileName();
  ASSERT_NE(base::FilePath::StringType::npos,
            filename.value().find(FILE_PATH_LITERAL("chrome_debug.log")));

  RestoreEnvironmentVariable();
}

// Tests the log file name getter with an environment variable.
TEST_F(ChromeLoggingTest, EnvironmentLogFileName) {
  SaveEnvironmentVariable("test value");

  base::FilePath filename = logging::GetLogFileName();
  ASSERT_EQ(base::FilePath(FILE_PATH_LITERAL("test value")).value(),
            filename.value());

  RestoreEnvironmentVariable();
}
