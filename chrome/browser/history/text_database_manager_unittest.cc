// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/text_database_manager.h"
#include "chrome/browser/history/visit_database.h"
#include "sql/connection.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;
using base::TimeTicks;

namespace history {

namespace {

const char* kURL1 = "http://www.google.com/asdf";
const char* kTitle1 = "Google A";
const char* kBody1 = "FOO page one.";

const char* kURL2 = "http://www.google.com/qwer";
const char* kTitle2 = "Google B";
const char* kBody2 = "FOO two.";

const char* kURL3 = "http://www.google.com/zxcv";
const char* kTitle3 = "Google C";
const char* kBody3 = "FOO drei";

const char* kURL4 = "http://www.google.com/hjkl";
const char* kTitle4 = "Google D";
const char* kBody4 = "FOO lalala four.";

const char* kURL5 = "http://www.google.com/uiop";
const char* kTitle5 = "Google cinq";
const char* kBody5 = "FOO page one.";

// This provides a simple implementation of a URL+VisitDatabase using an
// in-memory sqlite connection. The text database manager expects to be able to
// update the visit database to keep in sync.
class InMemDB : public URLDatabase, public VisitDatabase {
 public:
  InMemDB() {
    EXPECT_TRUE(db_.OpenInMemory());
    CreateURLTable(false);
    InitVisitTable();
  }
  virtual ~InMemDB() {
  }

 private:
  virtual sql::Connection& GetDB() OVERRIDE { return db_; }

  sql::Connection db_;

  DISALLOW_COPY_AND_ASSIGN(InMemDB);
};

// Adds all the pages once, and the first page once more in the next month.
// The times of all the pages will be filled into |*times|.
void AddAllPages(TextDatabaseManager& manager, VisitDatabase* visit_db,
                 std::vector<Time>* times) {
  Time::Exploded exploded;
  memset(&exploded, 0, sizeof(Time::Exploded));

  // Put the visits in two different months so it will query across databases.
  exploded.year = 2008;
  exploded.month = 1;
  exploded.day_of_month = 3;

  VisitRow visit_row;
  visit_row.url_id = 1;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_row.referring_visit = 0;
  visit_row.transition = content::PageTransitionFromInt(0);
  visit_row.segment_id = 0;
  visit_row.is_indexed = false;
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);

  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL1), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle1),
                      UTF8ToUTF16(kBody1));

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL2), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle2),
                      UTF8ToUTF16(kBody2));

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL3), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle3),
                      UTF8ToUTF16(kBody3));

  // Put the next ones in the next month.
  exploded.month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL4), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle4),
                      UTF8ToUTF16(kBody4));

  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL5), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle5),
                      UTF8ToUTF16(kBody5));

  // Put the first one in again in the second month.
  exploded.day_of_month++;
  visit_row.url_id = 2;
  visit_row.visit_time = Time::FromUTCExploded(exploded);
  visit_db->AddVisit(&visit_row, SOURCE_BROWSED);
  times->push_back(visit_row.visit_time);
  manager.AddPageData(GURL(kURL1), visit_row.url_id, visit_row.visit_id,
                      visit_row.visit_time, UTF8ToUTF16(kTitle1),
                      UTF8ToUTF16(kBody1));
}

bool ResultsHaveURL(const std::vector<TextDatabase::Match>& results,
                    const char* url) {
  GURL gurl(url);
  for (size_t i = 0; i < results.size(); i++) {
    if (results[i].url == gurl)
      return true;
  }
  return false;
}

}  // namespace

class TextDatabaseManagerTest : public testing::Test {
 public:
  // Called manually by the test so it can report failure to initialize.
  bool Init() {
    return file_util::CreateNewTempDirectory(
        FILE_PATH_LITERAL("TestSearchTest"), &dir_);
  }

 protected:
  virtual void SetUp() {
  }

  virtual void TearDown() {
    file_util::Delete(dir_, true);
  }

  MessageLoop message_loop_;

  // Directory containing the databases.
  base::FilePath dir_;
};

// Tests basic querying.
TEST_F(TextDatabaseManagerTest, InsertQuery) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                         &results, &first_time_searched);

  // We should have matched every page.
  EXPECT_EQ(6U, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));

  // The first time searched should have been the first page's time or before
  // (it could have eliminated some time for us).
  EXPECT_TRUE(first_time_searched <= times[0]);
}

// Tests that adding page components piecemeal will get them added properly.
// This does not supply a visit to update, this mode is used only by the unit
// tests right now, but we test it anyway.
TEST_F(TextDatabaseManagerTest, InsertCompleteNoVisit) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  // First add one without a visit.
  const GURL url(kURL1);
  manager.AddPageURL(url, 0, 0, Time::Now());
  manager.AddPageTitle(url, UTF8ToUTF16(kTitle1));
  manager.AddPageContents(url, UTF8ToUTF16(kBody1));

  // Check that the page got added.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                         &results, &first_time_searched);
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(kTitle1, UTF16ToUTF8(results[0].title));
}

// Like InsertCompleteNoVisit but specifies a visit to update. We check that the
// visit was updated properly.
TEST_F(TextDatabaseManagerTest, InsertCompleteVisit) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  // First add a visit to a page. We can just make up a URL ID since there is
  // not actually any URL database around.
  VisitRow visit;
  visit.url_id = 1;
  visit.visit_time = Time::Now();
  visit.referring_visit = 0;
  visit.transition = content::PAGE_TRANSITION_LINK;
  visit.segment_id = 0;
  visit.is_indexed = false;
  visit_db.AddVisit(&visit, SOURCE_BROWSED);

  // Add a full text indexed entry for that visit.
  const GURL url(kURL2);
  manager.AddPageURL(url, visit.url_id, visit.visit_id, visit.visit_time);
  manager.AddPageContents(url, UTF8ToUTF16(kBody2));
  manager.AddPageTitle(url, UTF8ToUTF16(kTitle2));

  // Check that the page got added.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                         &results, &first_time_searched);
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(kTitle2, UTF16ToUTF8(results[0].title));

  // Check that the visit got updated for its new indexed state.
  VisitRow out_visit;
  ASSERT_TRUE(visit_db.GetRowForVisit(visit.visit_id, &out_visit));
  EXPECT_TRUE(out_visit.is_indexed);
}

// Tests that partial inserts that expire are added to the database.
TEST_F(TextDatabaseManagerTest, InsertPartial) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  // Add the first one with just a URL.
  GURL url1(kURL1);
  manager.AddPageURL(url1, 0, 0, Time::Now());

  // Now add a second one with a URL and title.
  GURL url2(kURL2);
  manager.AddPageURL(url2, 0, 0, Time::Now());
  manager.AddPageTitle(url2, UTF8ToUTF16(kTitle2));

  // The third one has a URL and body.
  GURL url3(kURL3);
  manager.AddPageURL(url3, 0, 0, Time::Now());
  manager.AddPageContents(url3, UTF8ToUTF16(kBody3));

  // Expire stuff very fast. This assumes that the time between the first
  // AddPageURL and this line is less than the expiration time (20 seconds).
  TimeTicks added_time = TimeTicks::Now();
  TimeTicks expire_time = added_time + TimeDelta::FromSeconds(5);
  manager.FlushOldChangesForTime(expire_time);

  // Do a query, nothing should be added yet.
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(UTF8ToUTF16("google"), options,
                         &results, &first_time_searched);
  ASSERT_EQ(0U, results.size());

  // Compute a time threshold that will cause everything to be flushed, and
  // poke at the manager's internals to cause this to happen.
  expire_time = added_time + TimeDelta::FromDays(1);
  manager.FlushOldChangesForTime(expire_time);

  // Now we should have all 3 URLs added.
  manager.GetTextMatches(UTF8ToUTF16("google"), options,
                         &results, &first_time_searched);
  ASSERT_EQ(3U, results.size());
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
}

// Tests that partial inserts (due to timeouts) will still get updated if the
// data comes in later.
TEST_F(TextDatabaseManagerTest, PartialComplete) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  Time added_time = Time::Now();
  GURL url(kURL1);

  // We have to have the URL in the URL and visit databases for this test to
  // work.
  URLRow url_row(url);
  url_row.set_title(UTF8ToUTF16("chocolate"));
  URLID url_id = visit_db.AddURL(url_row);
  ASSERT_TRUE(url_id);
  VisitRow visit_row;
  visit_row.url_id = url_id;
  visit_row.visit_time = added_time;
  visit_db.AddVisit(&visit_row, SOURCE_BROWSED);

  // Add a URL with no title or body, and say that it expired.
  manager.AddPageURL(url, 0, 0, added_time);
  TimeTicks expire_time = TimeTicks::Now() + TimeDelta::FromDays(1);
  manager.FlushOldChangesForTime(expire_time);

  // Add the title. We should be able to query based on that. The title in the
  // URL row we set above should not come into the picture.
  manager.AddPageTitle(url, UTF8ToUTF16("Some unique title"));
  Time first_time_searched;
  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  manager.GetTextMatches(UTF8ToUTF16("unique"), options,
                         &results, &first_time_searched);
  EXPECT_EQ(1U, results.size());
  manager.GetTextMatches(UTF8ToUTF16("chocolate"), options,
                         &results, &first_time_searched);
  EXPECT_EQ(0U, results.size());

  // Now add the body, which should be queryable.
  manager.AddPageContents(url, UTF8ToUTF16("Very awesome body"));
  manager.GetTextMatches(UTF8ToUTF16("awesome"), options, &results, &first_time_searched);
  EXPECT_EQ(1U, results.size());

  // Adding the body will actually copy the title from the URL table rather
  // than the previously indexed row (we made them not match above). This isn't
  // necessarily what we want, but it's how it's implemented, and we don't want
  // to regress it.
  manager.GetTextMatches(UTF8ToUTF16("chocolate"), options, &results, &first_time_searched);
  EXPECT_EQ(1U, results.size());
}

// Tests that changes get properly committed to disk.
TEST_F(TextDatabaseManagerTest, Writing) {
  ASSERT_TRUE(Init());

  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  InMemDB visit_db;

  // Create the manager and write some stuff to it.
  {
    TextDatabaseManager manager(dir_, &visit_db, &visit_db);
    ASSERT_TRUE(manager.Init(NULL));

    std::vector<Time> times;
    AddAllPages(manager, &visit_db, &times);

    // We should have matched every page.
    manager.GetTextMatches(UTF8ToUTF16("FOO"), options, &results, &first_time_searched);
    EXPECT_EQ(6U, results.size());
  }
  results.clear();

  // Recreate the manager and make sure it finds the written stuff.
  {
    TextDatabaseManager manager(dir_, &visit_db, &visit_db);
    ASSERT_TRUE(manager.Init(NULL));

    // We should have matched every page again.
    manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                           &results, &first_time_searched);
    EXPECT_EQ(6U, results.size());
  }
}

// Tests that changes get properly committed to disk, as in the Writing test
// above, but when there is a transaction around the adds.
TEST_F(TextDatabaseManagerTest, WritingTransaction) {
  ASSERT_TRUE(Init());

  QueryOptions options;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;

  InMemDB visit_db;

  // Create the manager and write some stuff to it.
  {
    TextDatabaseManager manager(dir_, &visit_db, &visit_db);
    ASSERT_TRUE(manager.Init(NULL));

    std::vector<Time> times;
    manager.BeginTransaction();
    AddAllPages(manager, &visit_db, &times);
    // "Forget" to commit, it should be autocommittedd for us.

    // We should have matched every page.
    manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                           &results, &first_time_searched);
    EXPECT_EQ(6U, results.size());
  }
  results.clear();

  // Recreate the manager and make sure it finds the written stuff.
  {
    TextDatabaseManager manager(dir_, &visit_db, &visit_db);
    ASSERT_TRUE(manager.Init(NULL));

    // We should have matched every page again.
    manager.GetTextMatches(UTF8ToUTF16("FOO"), options,
                           &results, &first_time_searched);
    EXPECT_EQ(6U, results.size());
  }
}

// Tests querying where the maximum number of items is met.
TEST_F(TextDatabaseManagerTest, QueryMax) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  string16 foo = UTF8ToUTF16("FOO");

  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  options.max_count = 2;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);

  // We should have gotten the last two pages as results (the first page is
  // also the last).
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Asking for 4 pages, the first one should be in another DB.
  options.max_count = 4;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);

  EXPECT_EQ(4U, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));
}

// Tests querying backwards in time in chunks.
TEST_F(TextDatabaseManagerTest, QueryBackwards) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  std::vector<Time> times;
  AddAllPages(manager, &visit_db, &times);

  string16 foo = UTF8ToUTF16("FOO");

  // First do a query for all time, but with a max of 2. This will give us the
  // last two results and will tell us where to start searching when we want
  // to go back in time.
  QueryOptions options;
  options.begin_time = times[0] - TimeDelta::FromDays(100);
  options.end_time = times[times.size() - 1] + TimeDelta::FromDays(100);
  options.max_count = 2;
  std::vector<TextDatabase::Match> results;
  Time first_time_searched;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);

  // Check that we got the last two results.
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(first_time_searched <= times[4]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL5));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Query the previous two URLs and make sure we got the correct ones.
  options.end_time = first_time_searched;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(first_time_searched <= times[2]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL3));
  EXPECT_TRUE(ResultsHaveURL(results, kURL4));

  // Query the previous two URLs...
  options.end_time = first_time_searched;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(first_time_searched <= times[0]);
  EXPECT_TRUE(ResultsHaveURL(results, kURL2));
  EXPECT_TRUE(ResultsHaveURL(results, kURL1));

  // Try to query some more, there should be no results.
  options.end_time = first_time_searched;
  manager.GetTextMatches(foo, options, &results, &first_time_searched);
  EXPECT_EQ(0U, results.size());
}

// Tests deletion of uncommitted entries.
TEST_F(TextDatabaseManagerTest, DeleteUncommitted) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  manager.AddPageURL(GURL(kURL1), 0, 0, Time::FromInternalValue(1));
  manager.AddPageURL(GURL(kURL2), 0, 0, Time::FromInternalValue(2));
  manager.AddPageURL(GURL(kURL3), 0, 0, Time::FromInternalValue(3));
  manager.AddPageURL(GURL(kURL4), 0, 0, Time::FromInternalValue(4));
  manager.AddPageURL(GURL(kURL5), 0, 0, Time::FromInternalValue(5));

  EXPECT_EQ(5u, manager.GetUncommittedEntryCountForTest());

  // Should delete the first two entries.
  manager.DeleteFromUncommitted(std::set<GURL>(),
                                Time::FromInternalValue(1),
                                Time::FromInternalValue(3));

  EXPECT_EQ(3u, manager.GetUncommittedEntryCountForTest());

  // Should delete the third entry.
  {
    std::set<GURL> urls;
    urls.insert(GURL(kURL3));
    manager.DeleteFromUncommitted(urls, Time(), Time());
  }

  EXPECT_EQ(2u, manager.GetUncommittedEntryCountForTest());
}

// Tests deletion of uncommitted entries by time.
TEST_F(TextDatabaseManagerTest, DeleteUncommittedForTimes) {
  ASSERT_TRUE(Init());
  InMemDB visit_db;
  TextDatabaseManager manager(dir_, &visit_db, &visit_db);
  ASSERT_TRUE(manager.Init(NULL));

  manager.AddPageURL(GURL(kURL1), 0, 0, Time::FromInternalValue(2));
  manager.AddPageURL(GURL(kURL2), 0, 0, Time::FromInternalValue(3));
  manager.AddPageURL(GURL(kURL3), 0, 0, Time::FromInternalValue(4));
  manager.AddPageURL(GURL(kURL4), 0, 0, Time::FromInternalValue(5));
  manager.AddPageURL(GURL(kURL5), 0, 0, Time::FromInternalValue(6));

  EXPECT_EQ(5u, manager.GetUncommittedEntryCountForTest());

  std::vector<base::Time> times;
  times.push_back(Time::FromInternalValue(9));
  times.push_back(Time::FromInternalValue(7));
  times.push_back(Time::FromInternalValue(5));
  times.push_back(Time::FromInternalValue(5));
  times.push_back(Time::FromInternalValue(3));
  times.push_back(Time::FromInternalValue(1));
  manager.DeleteFromUncommittedForTimes(times);

  EXPECT_EQ(3u, manager.GetUncommittedEntryCountForTest());
}

}  // namespace history
