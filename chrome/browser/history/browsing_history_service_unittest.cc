// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/browsing_history_service.h"

#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

struct TestResult {
  std::string url;
  int64_t hour_offset;  // Visit time in hours past the baseline time.
};

// For each item in |results|, create a new Value representing the visit, and
// insert it into |list_value|.
void AddQueryResults(
    const base::Time baseline_time,
    TestResult* test_results,
    int test_results_size,
    std::vector<BrowsingHistoryService::HistoryEntry>* results) {
  for (int i = 0; i < test_results_size; ++i) {
    BrowsingHistoryService::HistoryEntry entry;
    entry.time =
        baseline_time + base::TimeDelta::FromHours(test_results[i].hour_offset);
    entry.url = GURL(test_results[i].url);
    entry.all_timestamps.insert(entry.time.ToInternalValue());
    results->push_back(entry);
  }
}

// Returns true if |result| matches the test data given by |correct_result|,
// otherwise returns false.
bool ResultEquals(const base::Time baseline_time,
                  const BrowsingHistoryService::HistoryEntry& result,
                  const TestResult& correct_result) {
  base::Time correct_time =
      baseline_time + base::TimeDelta::FromHours(correct_result.hour_offset);

  return result.time == correct_time && result.url == GURL(correct_result.url);
}

}  // namespace

class BrowsingHistoryServiceTest : public ::testing::Test {
 public:
  BrowsingHistoryServiceTest()
      : baseline_time_(base::Time::UnixEpoch().LocalMidnight()) {}
  ~BrowsingHistoryServiceTest() override {}

 protected:
  // Duplicates on the same day in the local timezone are removed, so set a
  // baseline time in local time.
  base::Time baseline_time_;
};

// Tests that the MergeDuplicateResults method correctly removes duplicate
// visits to the same URL on the same day.
TEST_F(BrowsingHistoryServiceTest, MergeDuplicateResults) {
  {
    // Basic test that duplicates on the same day are removed.
    TestResult test_data[] = {
        {"http://google.com", 0},
        {"http://google.de", 1},
        {"http://google.com", 2},
        {"http://google.com", 3}  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
  }

  {
    // Test that a duplicate URL on the next day is not removed.
    TestResult test_data[] = {
        {"http://google.com", 0},
        {"http://google.com", 23},
        {"http://google.com", 24},  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[2]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
  }

  {
    // Test multiple duplicates across multiple days.
    TestResult test_data[] = {
        // First day.
        {"http://google.de", 0},
        {"http://google.com", 1},
        {"http://google.de", 2},
        {"http://google.com", 3},

        // Second day.
        {"http://google.de", 24},
        {"http://google.com", 25},
        {"http://google.de", 26},
        {"http://google.com", 27},  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(4U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[7]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[6]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[2], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[3], test_data[2]));
  }

  {
    // Test that timestamps for duplicates are properly saved.
    TestResult test_data[] = {
        {"http://google.com", 0},
        {"http://google.de", 1},
        {"http://google.com", 2},
        {"http://google.com", 3}  // Most recent.
    };
    std::vector<BrowsingHistoryService::HistoryEntry> results;
    AddQueryResults(baseline_time_, test_data, arraysize(test_data), &results);
    BrowsingHistoryService::MergeDuplicateResults(&results);

    ASSERT_EQ(2U, results.size());
    EXPECT_TRUE(ResultEquals(baseline_time_, results[0], test_data[3]));
    EXPECT_TRUE(ResultEquals(baseline_time_, results[1], test_data[1]));
    EXPECT_EQ(3u, results[0].all_timestamps.size());
    EXPECT_EQ(1u, results[1].all_timestamps.size());
  }
}
