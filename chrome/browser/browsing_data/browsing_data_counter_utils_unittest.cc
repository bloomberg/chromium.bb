// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_counter_utils.h"

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browsing_data/autofill_counter.h"
#include "chrome/test/base/testing_browser_process.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(ENABLE_EXTENSIONS)
#include "base/strings/string_split.h"
#include "chrome/browser/browsing_data/hosted_apps_counter.h"
#endif

// Tests the complex output of the Autofill counter.
TEST(BrowsingDataCounterUtilsTest, AutofillCounterResult) {
  AutofillCounter counter;

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test all configurations of zero and nonzero partial results for datatypes.
  // Test singular and plural for each datatype.
  const struct TestCase {
    int num_credit_cards;
    int num_addresses;
    int num_suggestions;
    std::string expected_output;
  } kTestCases[] = {
      {0, 0, 0, "none"},
      {1, 0, 0, "1 credit card"},
      {0, 5, 0, "5 addresses"},
      {0, 0, 1, "1 suggestion"},
      {0, 0, 2, "2 suggestions"},
      {4, 7, 0, "4 credit cards, 7 addresses"},
      {3, 0, 9, "3 credit cards, 9 other suggestions"},
      {0, 1, 1, "1 address, 1 other suggestion"},
      {9, 6, 3, "9 credit cards, 6 addresses, 3 others"},
      {4, 2, 1, "4 credit cards, 2 addresses, 1 other"},
  };

  for (const TestCase& test_case : kTestCases) {
    AutofillCounter::AutofillResult result(
        &counter,
        test_case.num_suggestions,
        test_case.num_credit_cards,
        test_case.num_addresses);

    SCOPED_TRACE(base::StringPrintf(
        "Test params: %d credit card(s), "
            "%d address(es), %d suggestion(s).",
        test_case.num_credit_cards,
        test_case.num_addresses,
        test_case.num_suggestions
    ));

    base::string16 output = GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}

#if defined(ENABLE_EXTENSIONS)
// Tests the complex output of the hosted apps counter.
TEST(BrowsingDataCounterUtilsTest, HostedAppsCounterResult) {
  HostedAppsCounter counter;

  // This test assumes that the strings are served exactly as defined,
  // i.e. that the locale is set to the default "en".
  ASSERT_EQ("en", TestingBrowserProcess::GetGlobal()->GetApplicationLocale());

  // Test the output for various numbers of hosted apps.
  const struct TestCase {
    std::string apps_list;
    std::string expected_output;
  } kTestCases[] = {
      { "", "none" },
      { "App1", "1 app (App1)" },
      { "App1, App2", "2 apps (App1, App2)" },
      { "App1, App2, App3", "3 apps (App1, App2, and 1 more)" },
      { "App1, App2, App3, App4", "4 apps (App1, App2, and 2 more)" },
      { "App1, App2, App3, App4, App5", "5 apps (App1, App2, and 3 more)" },
  };

  for (const TestCase& test_case : kTestCases) {
    // Split the list of installed apps by commas.
    std::vector<std::string> apps = base::SplitString(
        test_case.apps_list, ",",
        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // The first two apps in the list are used as examples.
    std::vector<std::string> examples;
    examples.assign(
        apps.begin(), apps.begin() + (apps.size() > 2 ? 2 : apps.size()));

    HostedAppsCounter::HostedAppsResult result(
        &counter,
        apps.size(),
        examples);

    base::string16 output = GetCounterTextFromResult(&result);
    EXPECT_EQ(output, base::ASCIIToUTF16(test_case.expected_output));
  }
}
#endif
