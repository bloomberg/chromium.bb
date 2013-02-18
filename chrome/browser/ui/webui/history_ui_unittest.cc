// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "history_ui.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TestResult {
  std::string url;
  int64 hour_offset;  // Visit time in hours past the baseline time.
};

// Duplicates on the same day in the local timezone are removed, so set a
// baseline time in local time.
const base::Time baseline_time = base::Time::UnixEpoch().LocalMidnight();

// For each item in |results|, create a new Value representing the visit, and
// insert it into |list_value|.
void AddResultsToList(TestResult* results,
                      int results_size,
                      ListValue* list_value) {
  for (int i = 0; i < results_size; ++i) {
    DictionaryValue* result = new DictionaryValue;
    result->SetString("url", results[i].url);
    base::Time time =
        baseline_time + base::TimeDelta::FromHours(results[i].hour_offset);
    result->SetDouble("time", time.ToJsTime());
    list_value->Append(result);
  }
}

// Returns true if the result at |index| in |results| matches the test data
// given by |correct_result|, otherwise returns false.
bool ResultEquals(
    const ListValue& results, int index, TestResult correct_result) {
  const DictionaryValue* result;
  string16 url;
  double timestamp;

  if (results.GetDictionary(index, &result) &&
      result->GetDouble("time", &timestamp) &&
      result->GetString("url", &url)) {
    base::Time correct_time =
        baseline_time + base::TimeDelta::FromHours(correct_result.hour_offset);
    return base::Time::FromJsTime(timestamp) == correct_time &&
        url == ASCIIToUTF16(correct_result.url);
  }
  NOTREACHED();
  return false;
}

}  // namespace

// Tests that the RemoveDuplicateResults method correctly removes duplicate
// visits to the same URL on the same day.
TEST(HistoryUITest, RemoveDuplicateResults) {
  {
    // Basic test that duplicates on the same day are removed.
    TestResult test_data[] = {
      { "http://google.com", 0 },
      { "http://google.de", 1 },
      { "http://google.com", 2 },
      { "http://google.com", 3 }
    };
    ListValue results;
    AddResultsToList(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::RemoveDuplicateResults(&results);

    ASSERT_EQ(2U, results.GetSize());
    EXPECT_TRUE(ResultEquals(results, 0, test_data[0]));
    EXPECT_TRUE(ResultEquals(results, 1, test_data[1]));
  }

  {
    // Test that a duplicate URL on the next day is not removed.
    TestResult test_data[] = {
      { "http://google.com", 0 },
      { "http://google.com", 23 },
      { "http://google.com", 24 },
    };
    ListValue results;
    AddResultsToList(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::RemoveDuplicateResults(&results);

    ASSERT_EQ(2U, results.GetSize());
    EXPECT_TRUE(ResultEquals(results, 0, test_data[0]));
    EXPECT_TRUE(ResultEquals(results, 1, test_data[2]));
  }

  {
    // Test multiple duplicates across multiple days.
    TestResult test_data[] = {
      // First day.
      { "http://google.de", 0 },
      { "http://google.com", 1 },
      { "http://google.de", 2 },
      { "http://google.com", 3 },

      // Second day.
      { "http://google.de", 24 },
      { "http://google.com", 25 },
      { "http://google.de", 26 },
      { "http://google.com", 27 },
    };
    ListValue results;
    AddResultsToList(test_data, arraysize(test_data), &results);
    BrowsingHistoryHandler::RemoveDuplicateResults(&results);

    ASSERT_EQ(4U, results.GetSize());
    EXPECT_TRUE(ResultEquals(results, 0, test_data[0]));
    EXPECT_TRUE(ResultEquals(results, 1, test_data[1]));
    EXPECT_TRUE(ResultEquals(results, 2, test_data[4]));
    EXPECT_TRUE(ResultEquals(results, 3, test_data[5]));
  }
}
