// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/file_path.h>
#include <chrome/browser/policy/policy_path_parser.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyPathParserTests : public testing::Test {
 protected:
  void CheckForSubstitution(FilePath::StringType test_string,
                            FilePath::StringType var_name) {
    FilePath::StringType var(test_string);
    FilePath::StringType var_result =
        path_parser::ExpandPathVariables(var);
    ASSERT_EQ(var_result.find(var_name), FilePath::StringType::npos);
  }
};

TEST_F(PolicyPathParserTests, AllPlatformVariables) {
  // No vars whatsoever no substitution should occur.
  FilePath::StringType no_vars(FILE_PATH_LITERAL("//$C/shares"));
  FilePath::StringType no_vars_result =
      path_parser::ExpandPathVariables(no_vars);
  ASSERT_EQ(no_vars_result, no_vars);

  // This is unknown variable and shouldn't be substituted.
  FilePath::StringType unknown_vars(FILE_PATH_LITERAL("//$C/${buggy}"));
  FilePath::StringType unknown_vars_result =
      path_parser::ExpandPathVariables(unknown_vars);
  ASSERT_EQ(unknown_vars_result, unknown_vars);

  // Both should have been substituted.
  FilePath::StringType vars(FILE_PATH_LITERAL("${user_name}${machine_name}"));
  FilePath::StringType vars_result = path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            FilePath::StringType::npos);

  // Should substitute only one instance.
  vars = FILE_PATH_LITERAL("${machine_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  size_t pos = vars_result.find(FILE_PATH_LITERAL("${machine_name}"));
  ASSERT_NE(pos, FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}"), pos+1),
            FilePath::StringType::npos);

  vars =FILE_PATH_LITERAL("${user_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            FilePath::StringType::npos);

  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${user_name}"),
                       FILE_PATH_LITERAL("${user_name}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${machine_name}"),
                       FILE_PATH_LITERAL("${machine_name}"));
}

#if defined(OS_MACOSX)

TEST_F(PolicyPathParserTests, MacVariables) {
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${users}"),
                       FILE_PATH_LITERAL("${users}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${documents}"),
                       FILE_PATH_LITERAL("${documents}"));
}

#elif defined(OS_WIN)

TEST_F(PolicyPathParserTests, WinVariables) {
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${documents}"),
                       FILE_PATH_LITERAL("${documents}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${local_app_data}"),
                       FILE_PATH_LITERAL("${local_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${roaming_app_data}"),
                       FILE_PATH_LITERAL("${roaming_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${profile}"),
                       FILE_PATH_LITERAL("${profile}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${global_app_data}"),
                       FILE_PATH_LITERAL("${global_app_data}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${program_files}"),
                       FILE_PATH_LITERAL("${program_files}"));
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${windows}"),
                       FILE_PATH_LITERAL("${windows}"));
}

#endif  // OS_WIN

}  // namespace policy
