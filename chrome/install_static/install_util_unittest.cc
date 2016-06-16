// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/install_static/install_util.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace install_static {

// Tests the MatchPattern function in the install_static library.
TEST(InstallStaticTest, MatchPattern) {
  EXPECT_TRUE(MatchPattern(L"", L""));
  EXPECT_TRUE(MatchPattern(L"", L"*"));
  EXPECT_FALSE(MatchPattern(L"", L"*a"));
  EXPECT_FALSE(MatchPattern(L"", L"abc"));
  EXPECT_TRUE(MatchPattern(L"Hello1234", L"He??o*1*"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*?"));
  EXPECT_TRUE(MatchPattern(L"Foo", L"F*"));
  EXPECT_FALSE(MatchPattern(L"Foo", L"F*b"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*c*d"));
  EXPECT_TRUE(MatchPattern(L"abcd", L"*?c*d"));
  EXPECT_FALSE(MatchPattern(L"abcd", L"abcd*efgh"));
  EXPECT_TRUE(MatchPattern(L"foobarabc", L"*bar*"));
}

// Tests the TokenizeString function in the install_static library.
TEST(InstallStaticTest, TokenizeString) {
  // Test if the string is tokenized correctly with all tokens stripped of
  // leading and trailing spaces.
  std::vector<std::string> results =
      TokenizeString("un |deux\t|trois\n|quatre", '|', true);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("un", "deux", "trois", "quatre"));

  // Test if the string is tokenized correctly with all tokens having
  // leading and trailing spaces intact.
  results = TokenizeString("un |deux\t|trois\n|quatre", '|', false);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("un ", "deux\t", "trois\n", "quatre"));

  // Test if tokenize returns the original string if a string containing only
  // one token and no delimiters is passed.
  results = TokenizeString("un |deux\t|trois\n|quatre", '!', false);
  ASSERT_EQ(1u, results.size());
  ASSERT_EQ(results[0], "un |deux\t|trois\n|quatre");

  // Test if tokenize handles a space character as delimiter.
  results = TokenizeString("foo bar bleh blah boo", ' ', false);
  ASSERT_EQ(5u, results.size());
  EXPECT_THAT(results, ElementsAre("foo", "bar", "bleh", "blah", "boo"));

  // Test string with only delimiters.
  results = TokenizeString("||||", '|', false);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", ""));

  // Test string with spaces separated by delimiters.
  results = TokenizeString(" | | | |", '|', false);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre(" ", " ", " ", " "));

  results = TokenizeString("one|two||four", '|', false);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("one", "two", "", "four"));

  // TokenizeString16 tests.
  // Test if the string is tokenized correctly with all tokens stripped of
  // leading and trailing spaces.
  std::vector<base::string16> results16 =
      TokenizeString16(L"un |deux\t|trois\n|quatre", L'|', true);
  ASSERT_EQ(4u, results16.size());
  EXPECT_THAT(results16, ElementsAre(L"un", L"deux", L"trois", L"quatre"));

  // Test string with spaces separated by delimiters.
  results16 = TokenizeString16(L"one|two||four", L'|', false);
  ASSERT_EQ(4u, results16.size());
  EXPECT_THAT(results16, ElementsAre(L"one", L"two", L"", L"four"));
}

// Tests the CompareVersionString function in the install_static library.
TEST(InstallStaticTest, CompareVersions) {
  // Case 1. Invalid versions.
  int result = 0;
  EXPECT_FALSE(CompareVersionStrings("", "", &result));
  EXPECT_FALSE(CompareVersionStrings("0.0.0.0", "A.B.C.D", &result));
  EXPECT_FALSE(CompareVersionStrings("A.0.0.0", "0.0.0.0", &result));

  // Case 2. Equal versions.
  EXPECT_TRUE(CompareVersionStrings("0.0.0.0", "0.0.0.0", &result));
  EXPECT_EQ(0, result);

  // Case 3. Version1 > Version 2.
  EXPECT_TRUE(CompareVersionStrings("1.0.0.0", "0.0.0.0", &result));
  EXPECT_EQ(1, result);

  // Case 4. Version1 < Version 2.
  EXPECT_TRUE(CompareVersionStrings("0.0.0.0", "1.0.0.0", &result));
  EXPECT_EQ(-1, result);

  // Case 5. Version1 < Version 2.
  EXPECT_TRUE(CompareVersionStrings("0.0", "0.0.1.0", &result));
  EXPECT_EQ(-1, result);

  // Case 6. Version1 > Version 2.
  EXPECT_TRUE(CompareVersionStrings("0.0.0.2", "0.0", &result));
  EXPECT_EQ(1, result);

  // Case 7. Version1 > Version 2.
  EXPECT_TRUE(CompareVersionStrings("1.1.1.2", "1.1.1.1", &result));
  EXPECT_EQ(1, result);

  // Case 8. Version1 < Version2
  EXPECT_TRUE(CompareVersionStrings("0.0.0.2", "0.0.0.3", &result));
  EXPECT_EQ(-1, result);

  // Case 9. Version1 > Version2
  EXPECT_TRUE(CompareVersionStrings("0.0.0.4", "0.0.0.3", &result));
  EXPECT_EQ(1, result);

  // Case 10. Version1 > Version2. Multiple digit numbers.
  EXPECT_TRUE(CompareVersionStrings("0.0.12.1", "0.0.10.3", &result));
  EXPECT_EQ(1, result);

  // Case 11. Version1 < Version2. Multiple digit number.
  EXPECT_TRUE(CompareVersionStrings("10.11.12.13", "12.11.12.13", &result));
  EXPECT_EQ(-1, result);
}

// Tests the install_static::GetSwitchValueFromCommandLine function.
TEST(InstallStaticTest, GetSwitchValueFromCommandLineTest) {
  // Simple case with one switch.
  std::string value =
      GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type=bar", "type");
  EXPECT_EQ("bar", value);

  // Multiple switches with trailing spaces between them.
  value = GetSwitchValueFromCommandLine(
      "c:\\temp\\bleh.exe --type=bar  --abc=def bleh", "abc");
  EXPECT_EQ("def", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      "c:\\temp\\bleh.exe --type=bar \t\t\t --abc=def bleh", "abc");
  EXPECT_EQ("def", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine(
      "c:\\temp\\bleh.exe --foo=bar  --abc=def bleh", "type");
  EXPECT_EQ("", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe", "type");
  EXPECT_EQ("", value);

  // Non existent switch.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe type=bar", "type");
  EXPECT_EQ("", value);

  // Trailing spaces after the switch.
  value = GetSwitchValueFromCommandLine(
      "c:\\temp\\bleh.exe --type=bar      \t\t", "type");
  EXPECT_EQ("bar", value);

  // Multiple switches with trailing spaces and tabs between them.
  value = GetSwitchValueFromCommandLine(
      "c:\\temp\\bleh.exe --type=bar      \t\t --foo=bleh", "foo");
  EXPECT_EQ("bleh", value);

  // Nothing after a switch.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type=", "type");
  EXPECT_TRUE(value.empty());

  // Whitespace after a switch.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type= ", "type");
  EXPECT_TRUE(value.empty());

  // Just tabs after a switch.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type=\t\t\t",
      "type");
  EXPECT_TRUE(value.empty());

  // Whitespace after the "=" before the value.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type= bar",
      "type");
  EXPECT_EQ("bar", value);

  // Tabs after the "=" before the value.
  value = GetSwitchValueFromCommandLine("c:\\temp\\bleh.exe --type=\t\t\tbar",
      "type");
  EXPECT_EQ(value, "bar");
}

}  // namespace install_static