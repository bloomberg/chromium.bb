// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "chrome/browser/policy/policy_path_parser.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

class PolicyPathParserTests : public testing::Test {
 protected:
  void CheckForSubstitution(base::FilePath::StringType test_string,
                            base::FilePath::StringType var_name) {
    base::FilePath::StringType var(test_string);
    base::FilePath::StringType var_result =
        path_parser::ExpandPathVariables(var);
    ASSERT_EQ(var_result.find(var_name), base::FilePath::StringType::npos);
  }
};

namespace path_parser {

// The test needs access to a routine that is not exposed via the public header.
void ReplaceVariableInPathWithValue(
    const base::FilePath::StringType& variable,
    const policy::path_parser::internal::GetValueFuncPtr& value_func_ptr,
    base::FilePath::StringType* path);

}  // namespace path_parser;

bool GetBuggy(base::FilePath::StringType* value) {
  *value = base::FilePath::StringType(FILE_PATH_LITERAL("ok"));
  return true;
}

TEST_F(PolicyPathParserTests, ReplaceVariableInPathWithValue) {
  // This is custom variable with custom callback. It should replace ${buggy}
  // with ok.
  base::FilePath::StringType custom_vars(FILE_PATH_LITERAL("//$C/${buggy}"));
  base::FilePath::StringType custom_vars_expected(FILE_PATH_LITERAL("//$C/ok"));
  base::FilePath::StringType buggy(FILE_PATH_LITERAL("${buggy}"));

  path_parser::ReplaceVariableInPathWithValue(buggy, &GetBuggy, &custom_vars);
  ASSERT_EQ(custom_vars, custom_vars_expected);

  // There is no ${buggy} in input, so it should remain unchanged.
  base::FilePath::StringType custom2_vars(FILE_PATH_LITERAL("//$C/ok"));
  base::FilePath::StringType custom2_vars_expected(
      FILE_PATH_LITERAL("//$C/ok"));

  path_parser::ReplaceVariableInPathWithValue(buggy, &GetBuggy, &custom2_vars);
  ASSERT_EQ(custom2_vars, custom2_vars_expected);
}

TEST_F(PolicyPathParserTests, CommonExpandPathVariables) {
  // No vars whatsoever no substitution should occur.
  base::FilePath::StringType no_vars(FILE_PATH_LITERAL("//$C/shares"));
  base::FilePath::StringType no_vars_result =
      path_parser::ExpandPathVariables(no_vars);
  ASSERT_EQ(no_vars_result, no_vars);

  // This is unknown variable and shouldn't be substituted.
  base::FilePath::StringType unknown_vars(FILE_PATH_LITERAL("//$C/${buggy}"));
  base::FilePath::StringType unknown_vars_result =
      path_parser::ExpandPathVariables(unknown_vars);
  ASSERT_EQ(unknown_vars_result, unknown_vars);

  // Trim quotes around, but not inside paths. Test against bug 80211.
  base::FilePath::StringType no_quotes(FILE_PATH_LITERAL("//$C/\"a\"/$path"));
  base::FilePath::StringType single_quotes(
      FILE_PATH_LITERAL("'//$C/\"a\"/$path'"));
  base::FilePath::StringType double_quotes(
      FILE_PATH_LITERAL("\"//$C/\"a\"/$path\""));
  base::FilePath::StringType quotes_result =
      path_parser::ExpandPathVariables(single_quotes);
  ASSERT_EQ(quotes_result, no_quotes);
  quotes_result = path_parser::ExpandPathVariables(double_quotes);
  ASSERT_EQ(quotes_result, no_quotes);
}

#if defined(OS_MACOSX)
// http://crbug.com/327520
#define MAYBE_AllPlatformVariables DISABLED_AllPlatformVariables
#else
#define MAYBE_AllPlatformVariables AllPlatformVariables
#endif
TEST_F(PolicyPathParserTests, MAYBE_AllPlatformVariables) {
  // Both should have been substituted.
  base::FilePath::StringType vars(
      FILE_PATH_LITERAL("${user_name}${machine_name}"));
  base::FilePath::StringType vars_result =
      path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            base::FilePath::StringType::npos);

  // Should substitute only one instance.
  vars = FILE_PATH_LITERAL("${machine_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  size_t pos = vars_result.find(FILE_PATH_LITERAL("${machine_name}"));
  ASSERT_NE(pos, base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}"), pos+1),
            base::FilePath::StringType::npos);

  vars =FILE_PATH_LITERAL("${user_name}${machine_name}");
  vars_result = path_parser::ExpandPathVariables(vars);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${user_name}")),
            base::FilePath::StringType::npos);
  ASSERT_EQ(vars_result.find(FILE_PATH_LITERAL("${machine_name}")),
            base::FilePath::StringType::npos);

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
  CheckForSubstitution(FILE_PATH_LITERAL("//$C/${client_name}"),
                       FILE_PATH_LITERAL("${client_name}"));
}

#endif  // OS_WIN

}  // namespace policy
