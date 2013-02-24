// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

bool IsVisitInfoEqual(const VisitRow& a,
                      const VisitRow& b) {
  return a.visit_id == b.visit_id &&
         a.url_id == b.url_id &&
         a.visit_time == b.visit_time &&
         a.referring_visit == b.referring_visit &&
         a.transition == b.transition &&
         a.is_indexed == b.is_indexed;
}

}  // namespace

class VisitDatabaseTest : public PlatformTest,
                          public URLDatabase,
                          public VisitDatabase {
 public:
  VisitDatabaseTest() {
  }

 private:
  // Test setup.
  virtual void SetUp() {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file = temp_dir_.path().AppendASCII("VisitTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitVisitTable();
  }
  virtual void TearDown() {
    db_.Close();
    PlatformTest::TearDown();
  }

  // Provided for URL/VisitDatabase.
  virtual sql::Connection& GetDB() OVERRIDE {
    return db_;
  }

  base::ScopedTempDir temp_dir_;
  sql::Connection db_;
};

TEST_F(VisitDatabaseTest, Add) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0, content::PAGE_TRANSITION_LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      content::PAGE_TRANSITION_TYPED, 0);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      content::PAGE_TRANSITION_LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info3, SOURCE_BROWSED));

  // Query the first two.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());

  // Make sure we got both (order in result set is visit time).
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2));
}

TEST_F(VisitDatabaseTest, Delete) {
  // Add three visits that form a chain of navigation, and then delete the
  // middle one. We should be left with the outer two visits, and the chain
  // should link them.
  static const int kTime1 = 1000;
  VisitRow visit_info1(1, Time::FromInternalValue(kTime1), 0,
                       content::PAGE_TRANSITION_LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  static const int kTime2 = kTime1 + 1;
  VisitRow visit_info2(1, Time::FromInternalValue(kTime2),
                       visit_info1.visit_id, content::PAGE_TRANSITION_LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  static const int kTime3 = kTime2 + 1;
  VisitRow visit_info3(1, Time::FromInternalValue(kTime3),
                       visit_info2.visit_id, content::PAGE_TRANSITION_LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info3, SOURCE_BROWSED));

  // First make sure all the visits are there.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(3), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2) &&
              IsVisitInfoEqual(matches[2], visit_info3));

  // Delete the middle one.
  DeleteVisit(visit_info2);

  // The outer two should be left, and the last one should have the first as
  // the referrer.
  visit_info3.referring_visit = visit_info1.visit_id;
  matches.clear();
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info3));
}

TEST_F(VisitDatabaseTest, Update) {
  // Make something in the database.
  VisitRow original(1, Time::Now(), 23, content::PageTransitionFromInt(0), 19);
  AddVisit(&original, SOURCE_BROWSED);

  // Mutate that row.
  VisitRow modification(original);
  modification.url_id = 2;
  modification.transition = content::PAGE_TRANSITION_TYPED;
  modification.visit_time = Time::Now() + TimeDelta::FromDays(1);
  modification.referring_visit = 9292;
  modification.is_indexed = true;
  UpdateVisitRow(modification);

  // Check that the mutated version was written.
  VisitRow final;
  GetRowForVisit(original.visit_id, &final);
  EXPECT_TRUE(IsVisitInfoEqual(modification, final));
}

// TODO(brettw) write test for GetMostRecentVisitForURL!

namespace {

std::vector<VisitRow> GetTestVisitRows() {
  // Tests can be sensitive to the local timezone, so use a local time as the
  // basis for all visit times.
  base::Time base_time = Time::UnixEpoch().LocalMidnight();

  // Add one visit.
  VisitRow visit_info1(1, base_time + TimeDelta::FromMinutes(1), 0,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_LINK |
          content::PAGE_TRANSITION_CHAIN_START |
          content::PAGE_TRANSITION_CHAIN_END),
      0);
  visit_info1.visit_id = 1;

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_TYPED |
          content::PAGE_TRANSITION_CHAIN_START |
          content::PAGE_TRANSITION_CHAIN_END),
      0);
  visit_info2.visit_id = 2;

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_LINK |
          content::PAGE_TRANSITION_CHAIN_START),
      0);
  visit_info3.visit_id = 3;

  // Add a redirect visit from the last page.
  VisitRow visit_info4(3,
      visit_info1.visit_time + TimeDelta::FromSeconds(3), visit_info3.visit_id,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_SERVER_REDIRECT |
          content::PAGE_TRANSITION_CHAIN_END),
      0);
  visit_info4.visit_id = 4;

  // Add a subframe visit.
  VisitRow visit_info5(4,
      visit_info1.visit_time + TimeDelta::FromSeconds(4), visit_info4.visit_id,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_AUTO_SUBFRAME |
          content::PAGE_TRANSITION_CHAIN_START |
          content::PAGE_TRANSITION_CHAIN_END),
      0);
  visit_info5.visit_id = 5;

  // Add third visit for the same URL as visit 1 and 2, but exactly a day
  // later than visit 2.
  VisitRow visit_info6(visit_info1.url_id,
      visit_info2.visit_time + TimeDelta::FromDays(1), 1,
      static_cast<content::PageTransition>(
          content::PAGE_TRANSITION_TYPED |
          content::PAGE_TRANSITION_CHAIN_START |
          content::PAGE_TRANSITION_CHAIN_END),
      0);
  visit_info6.visit_id = 6;

  std::vector<VisitRow> test_visit_rows;
  test_visit_rows.push_back(visit_info1);
  test_visit_rows.push_back(visit_info2);
  test_visit_rows.push_back(visit_info3);
  test_visit_rows.push_back(visit_info4);
  test_visit_rows.push_back(visit_info5);
  test_visit_rows.push_back(visit_info6);
  return test_visit_rows;
}

}  // namespace

TEST_F(VisitDatabaseTest, GetVisitsForTimes) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for all our times.  We should get all visits.
  {
    std::vector<base::Time> times;
    for (size_t i = 0; i < test_visit_rows.size(); ++i) {
      times.push_back(test_visit_rows[i].visit_time);
    }
    VisitVector results;
    GetVisitsForTimes(times, &results);
    EXPECT_EQ(test_visit_rows.size(), results.size());
  }

  // Query the visits for a single time.
  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    std::vector<base::Time> times;
    times.push_back(test_visit_rows[i].visit_time);
    VisitVector results;
    GetVisitsForTimes(times, &results);
    ASSERT_EQ(static_cast<size_t>(1), results.size());
    EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[i]));
  }
}

TEST_F(VisitDatabaseTest, GetAllVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for all time.  We should get all visits.
  VisitVector results;
  GetAllVisitsInRange(Time(), Time(), 0, &results);
  ASSERT_EQ(test_visit_rows.size(), results.size());
  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(IsVisitInfoEqual(results[i], test_visit_rows[i]));
  }

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  GetAllVisitsInRange(test_visit_rows[1].visit_time,
                      test_visit_rows[3].visit_time, 0,
                      &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[1]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[2]));

  // Query for a max count and make sure we get only that number.
  GetAllVisitsInRange(Time(), Time(), 1, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[0]));
}

TEST_F(VisitDatabaseTest, GetVisibleVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for all time.  We should not get the first or the second
  // visit (duplicates of the sixth) or the redirect or subframe visits.
  VisitVector results;
  QueryOptions options;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[3]));

  // Now try with only per-day de-duping -- the second visit should appear,
  // since it's a duplicate of visit6 but on a different day.
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(3), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[3]));
  EXPECT_TRUE(IsVisitInfoEqual(results[2], test_visit_rows[1]));

  // Set the end time to exclude the second visit. The first visit should be
  // returned. Even though the second is a more recent visit, it's not in the
  // query range.
  options.end_time = test_visit_rows[1].visit_time;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[0]));

  options = QueryOptions();  // Reset to options to default.

  // Query for a max count and make sure we get only that number.
  options.max_count = 1;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  options.begin_time = test_visit_rows[1].visit_time;
  options.end_time = test_visit_rows[3].visit_time;
  options.max_count = 0;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[1]));
}

TEST_F(VisitDatabaseTest, VisitSource) {
  // Add visits.
  VisitRow visit_info1(111, Time::Now(), 0, content::PAGE_TRANSITION_LINK, 0);
  ASSERT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  VisitRow visit_info2(112, Time::Now(), 1, content::PAGE_TRANSITION_TYPED, 0);
  ASSERT_TRUE(AddVisit(&visit_info2, SOURCE_SYNCED));

  VisitRow visit_info3(113, Time::Now(), 0, content::PAGE_TRANSITION_TYPED, 0);
  ASSERT_TRUE(AddVisit(&visit_info3, SOURCE_EXTENSION));

  // Query each visit.
  std::vector<VisitRow> matches;
  ASSERT_TRUE(GetVisitsForURL(111, &matches));
  ASSERT_EQ(1U, matches.size());
  VisitSourceMap sources;
  GetVisitsSource(matches, &sources);
  EXPECT_EQ(0U, sources.size());

  ASSERT_TRUE(GetVisitsForURL(112, &matches));
  ASSERT_EQ(1U, matches.size());
  GetVisitsSource(matches, &sources);
  ASSERT_EQ(1U, sources.size());
  EXPECT_EQ(SOURCE_SYNCED, sources[matches[0].visit_id]);

  ASSERT_TRUE(GetVisitsForURL(113, &matches));
  ASSERT_EQ(1U, matches.size());
  GetVisitsSource(matches, &sources);
  ASSERT_EQ(1U, sources.size());
  EXPECT_EQ(SOURCE_EXTENSION, sources[matches[0].visit_id]);
}

TEST_F(VisitDatabaseTest, GetIndexedVisits) {
  // Add non-indexed visits.
  int url_id = 111;
  VisitRow visit_info1(
      url_id, Time::Now(), 0, content::PAGE_TRANSITION_LINK, 0);
  ASSERT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  VisitRow visit_info2(
      url_id, Time::Now(), 0, content::PAGE_TRANSITION_TYPED, 0);
  ASSERT_TRUE(AddVisit(&visit_info2, SOURCE_SYNCED));

  std::vector<VisitRow> visits;
  EXPECT_TRUE(GetVisitsForURL(url_id, &visits));
  EXPECT_EQ(static_cast<size_t>(2), visits.size());
  EXPECT_TRUE(GetIndexedVisitsForURL(url_id, &visits));
  EXPECT_EQ(static_cast<size_t>(0), visits.size());

  VisitRow visit_info3(
      url_id, Time::Now(), 2, content::PAGE_TRANSITION_TYPED, 0);
  visit_info3.is_indexed = true;
  ASSERT_TRUE(AddVisit(&visit_info3, SOURCE_SYNCED));
  EXPECT_TRUE(GetVisitsForURL(url_id, &visits));
  EXPECT_EQ(static_cast<size_t>(3), visits.size());
  EXPECT_TRUE(GetIndexedVisitsForURL(url_id, &visits));
  EXPECT_EQ(static_cast<size_t>(1), visits.size());
}

}  // namespace history
