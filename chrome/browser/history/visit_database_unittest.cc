// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/browser/history/visit_database.h"
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
  void SetUp() {
    PlatformTest::SetUp();
    FilePath temp_dir;
    PathService::Get(base::DIR_TEMP, &temp_dir);
    db_file_ = temp_dir.AppendASCII("VisitTest.db");
    file_util::Delete(db_file_, false);

    EXPECT_TRUE(db_.Open(db_file_));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitVisitTable();
  }
  void TearDown() {
    db_.Close();
    file_util::Delete(db_file_, false);
    PlatformTest::TearDown();
  }

  // Provided for URL/VisitDatabase.
  virtual sql::Connection& GetDB() {
    return db_;
  }

  FilePath db_file_;
  sql::Connection db_;
};

TEST_F(VisitDatabaseTest, Add) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0, PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      PageTransition::TYPED, 0);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      PageTransition::LINK, 0);
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
                       PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  static const int kTime2 = kTime1 + 1;
  VisitRow visit_info2(1, Time::FromInternalValue(kTime2),
                       visit_info1.visit_id, PageTransition::LINK, 0);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  static const int kTime3 = kTime2 + 1;
  VisitRow visit_info3(1, Time::FromInternalValue(kTime3),
                       visit_info2.visit_id, PageTransition::LINK, 0);
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
  VisitRow original(1, Time::Now(), 23, 22, 19);
  AddVisit(&original, SOURCE_BROWSED);

  // Mutate that row.
  VisitRow modification(original);
  modification.url_id = 2;
  modification.transition = PageTransition::TYPED;
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

TEST_F(VisitDatabaseTest, GetVisibleVisitsInRange) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0,
      static_cast<PageTransition::Type>(PageTransition::LINK |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info1.visit_id = 1;
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
      visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      static_cast<PageTransition::Type>(PageTransition::TYPED |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info2.visit_id = 2;
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  // Add third visit for a different page.
  VisitRow visit_info3(2,
      visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      static_cast<PageTransition::Type>(PageTransition::LINK |
                                        PageTransition::CHAIN_START),
      0);
  visit_info3.visit_id = 3;
  EXPECT_TRUE(AddVisit(&visit_info3, SOURCE_BROWSED));

  // Add a redirect visit from the last page.
  VisitRow visit_info4(3,
      visit_info1.visit_time + TimeDelta::FromSeconds(3), visit_info3.visit_id,
      static_cast<PageTransition::Type>(PageTransition::SERVER_REDIRECT |
                                        PageTransition::CHAIN_END),
      0);
  visit_info4.visit_id = 4;
  EXPECT_TRUE(AddVisit(&visit_info4, SOURCE_BROWSED));

  // Add a subframe visit.
  VisitRow visit_info5(4,
      visit_info1.visit_time + TimeDelta::FromSeconds(4), visit_info4.visit_id,
      static_cast<PageTransition::Type>(PageTransition::AUTO_SUBFRAME |
                                        PageTransition::CHAIN_START |
                                        PageTransition::CHAIN_END),
      0);
  visit_info5.visit_id = 5;
  EXPECT_TRUE(AddVisit(&visit_info5, SOURCE_BROWSED));

  // Query the visits for all time, we should not get the first (duplicate of
  // the second) or the redirect or subframe visits.
  VisitVector results;
  GetVisibleVisitsInRange(Time(), Time(), 0, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info4) &&
              IsVisitInfoEqual(results[1], visit_info2));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  GetVisibleVisitsInRange(visit_info2.visit_time, visit_info4.visit_time, 0,
                          &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info2));

  // Query for a max count and make sure we get only that number.
  GetVisibleVisitsInRange(Time(), Time(), 1, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], visit_info4));
}

TEST_F(VisitDatabaseTest, VisitSource) {
  // Add visits.
  VisitRow visit_info1(111, Time::Now(), 0, PageTransition::LINK, 0);
  ASSERT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  VisitRow visit_info2(112, Time::Now(), 1, PageTransition::TYPED, 0);
  ASSERT_TRUE(AddVisit(&visit_info2, SOURCE_SYNCED));

  VisitRow visit_info3(113, Time::Now(), 0, PageTransition::TYPED, 0);
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

}  // namespace history
