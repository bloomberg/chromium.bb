// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

// Tests the history service for querying functionality.

namespace history {

namespace {

struct TestEntry {
  const char* url;
  const char* title;
  const int days_ago;
  const char* body;
  Time time;  // Filled by SetUp.
} test_entries[] = {
  // This one is visited super long ago so it will be in a different database
  // from the next appearance of it at the end.
  {"http://example.com/", "Other", 180, "Other"},

  // These are deliberately added out of chronological order. The history
  // service should sort them by visit time when returning query results.
  // The correct index sort order is 4 2 3 1 7 6 5 0.
  {"http://www.google.com/1", "Title 1", 10,
   "PAGEONE FOO some body text"},
  {"http://www.google.com/3", "Title 3", 8,
   "PAGETHREE BAR some hello world for you"},
  {"http://www.google.com/2", "Title 2", 9,
   "PAGETWO FOO some more blah blah blah Title"},

  // A more recent visit of the first one.
  {"http://example.com/", "Other", 6, "Other"},

  {"http://www.google.com/6", "Title 6", 13, "I'm the second oldest"},
  {"http://www.google.com/4", "Title 4", 12, "four"},
  {"http://www.google.com/5", "Title 5", 11, "five"},
};

// Returns true if the nth result in the given results set matches. It will
// return false on a non-match or if there aren't enough results.
bool NthResultIs(const QueryResults& results,
                 int n,  // Result index to check.
                 int test_entry_index) {  // Index of test_entries to compare.
  if (static_cast<int>(results.size()) <= n)
    return false;

  const URLResult& result = results[n];

  // Check the visit time.
  if (result.visit_time() != test_entries[test_entry_index].time)
    return false;

  // Now check the URL & title.
  return result.url() == GURL(test_entries[test_entry_index].url) &&
         result.title() == UTF8ToUTF16(test_entries[test_entry_index].title);
}

}  // namespace

class HistoryQueryTest : public testing::Test {
 public:
  HistoryQueryTest() : page_id_(0) {
  }

  // Acts like a synchronous call to history's QueryHistory.
  void QueryHistory(const std::string& text_query,
                    const QueryOptions& options,
                    QueryResults* results) {
    history_->QueryHistory(
        UTF8ToUTF16(text_query), options, &consumer_,
        base::Bind(&HistoryQueryTest::QueryHistoryComplete,
                   base::Unretained(this)));
    MessageLoop::current()->Run();  // Will go until ...Complete calls Quit.
    results->Swap(&last_query_results_);
  }

  // Test paging through results, with a fixed number of results per page.
  // Defined here so code can be shared for the FTS version and the non-FTS
  // version.
  void TestPaging(const std::string& query_text,
                  const int* expected_results,
                  int results_length) {
    ASSERT_TRUE(history_.get());

    QueryOptions options;
    QueryResults results;

    options.max_count = 1;
    for (int i = 0; i < results_length; i++) {
      SCOPED_TRACE(testing::Message() << "i = " << i);
      QueryHistory(query_text, options, &results);
      ASSERT_EQ(1U, results.size());
      EXPECT_TRUE(NthResultIs(results, 0, expected_results[i]));
      options.end_time = results.back().visit_time();
    }
    QueryHistory(query_text, options, &results);
    EXPECT_EQ(0U, results.size());

    // Try with a max_count > 1.
    options.max_count = 2;
    options.end_time = base::Time();
    for (int i = 0; i < results_length / 2; i++) {
      SCOPED_TRACE(testing::Message() << "i = " << i);
      QueryHistory(query_text, options, &results);
      ASSERT_EQ(2U, results.size());
      EXPECT_TRUE(NthResultIs(results, 0, expected_results[i * 2]));
      EXPECT_TRUE(NthResultIs(results, 1, expected_results[i * 2 + 1]));
      options.end_time = results.back().visit_time();
    }

    // Add a couple of entries with duplicate timestamps. Use |query_text| as
    // the body of both entries so that they match a full-text query.
    TestEntry duplicates[] = {
      { "http://www.google.com/x", "", 1, query_text.c_str() },
      { "http://www.google.com/y", "", 1, query_text.c_str() }
    };
    AddEntryToHistory(duplicates[0]);
    AddEntryToHistory(duplicates[1]);

    // Make sure that paging proceeds even if there are duplicate timestamps.
    options.end_time = base::Time();
    do {
      QueryHistory(query_text, options, &results);
      ASSERT_NE(options.end_time, results.back().visit_time());
      options.end_time = results.back().visit_time();
    } while (!results.reached_beginning());
  }

 protected:
  scoped_ptr<HistoryService> history_;

  // Counter used to generate a unique ID for each page added to the history.
  int32 page_id_;

  void AddEntryToHistory(const TestEntry& entry) {
    // We need the ID scope and page ID so that the visit tracker can find it.
    const void* id_scope = reinterpret_cast<void*>(1);
    GURL url(entry.url);

    history_->AddPage(url, entry.time, id_scope, page_id_++, GURL(),
                      history::RedirectList(), content::PAGE_TRANSITION_LINK,
                      history::SOURCE_BROWSED, false);
    history_->SetPageTitle(url, UTF8ToUTF16(entry.title));
    history_->SetPageContents(url, UTF8ToUTF16(entry.body));
  }

 private:
  virtual void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    history_dir_ = temp_dir_.path().AppendASCII("HistoryTest");
    ASSERT_TRUE(file_util::CreateDirectory(history_dir_));

    history_.reset(new HistoryService);
    if (!history_->Init(history_dir_, NULL)) {
      history_.reset();  // Tests should notice this NULL ptr & fail.
      return;
    }

    // Fill the test data.
    Time now = Time::Now().LocalMidnight();
    for (size_t i = 0; i < arraysize(test_entries); i++) {
      test_entries[i].time =
          now - (test_entries[i].days_ago * TimeDelta::FromDays(1));
      AddEntryToHistory(test_entries[i]);
    }
  }

  virtual void TearDown() {
    if (history_.get()) {
      history_->SetOnBackendDestroyTask(MessageLoop::QuitClosure());
      history_->Cleanup();
      history_.reset();
      MessageLoop::current()->Run();  // Wait for the other thread.
    }
  }

  void QueryHistoryComplete(HistoryService::Handle, QueryResults* results) {
    results->Swap(&last_query_results_);
    MessageLoop::current()->Quit();  // Will return out to QueryHistory.
  }

  base::ScopedTempDir temp_dir_;

  MessageLoop message_loop_;

  base::FilePath history_dir_;

  CancelableRequestConsumer consumer_;

  // The QueryHistoryComplete callback will put the results here so QueryHistory
  // can return them.
  QueryResults last_query_results_;

  DISALLOW_COPY_AND_ASSIGN(HistoryQueryTest);
};

TEST_F(HistoryQueryTest, Basic) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Test duplicate collapsing. 0 is an older duplicate of 4, and should not
  // appear in the result set.
  QueryHistory(std::string(), options, &results);
  EXPECT_EQ(7U, results.size());

  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 2));
  EXPECT_TRUE(NthResultIs(results, 2, 3));
  EXPECT_TRUE(NthResultIs(results, 3, 1));
  EXPECT_TRUE(NthResultIs(results, 4, 7));
  EXPECT_TRUE(NthResultIs(results, 5, 6));
  EXPECT_TRUE(NthResultIs(results, 6, 5));

  // Next query a time range. The beginning should be inclusive, the ending
  // should be exclusive.
  options.begin_time = test_entries[3].time;
  options.end_time = test_entries[2].time;
  QueryHistory(std::string(), options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));
}

// Tests max_count feature for basic (non-Full Text Search) queries.
TEST_F(HistoryQueryTest, BasicCount) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  options.max_count = 2;
  QueryHistory(std::string(), options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
  EXPECT_TRUE(NthResultIs(results, 1, 2));
}

TEST_F(HistoryQueryTest, ReachedBeginning) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  QueryHistory(std::string(), options, &results);
  EXPECT_TRUE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_TRUE(results.reached_beginning());

  options.begin_time = test_entries[1].time;
  QueryHistory(std::string(), options, &results);
  EXPECT_FALSE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_FALSE(results.reached_beginning());

  // Try |begin_time| just later than the oldest visit.
  options.begin_time = test_entries[0].time + TimeDelta::FromMicroseconds(1);
  QueryHistory(std::string(), options, &results);
  EXPECT_FALSE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_FALSE(results.reached_beginning());

  // Try |begin_time| equal to the oldest visit.
  options.begin_time = test_entries[0].time;
  QueryHistory(std::string(), options, &results);
  EXPECT_TRUE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_TRUE(results.reached_beginning());

  // Try |begin_time| just earlier than the oldest visit.
  options.begin_time = test_entries[0].time - TimeDelta::FromMicroseconds(1);
  QueryHistory(std::string(), options, &results);
  EXPECT_TRUE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_TRUE(results.reached_beginning());

  // Test with |max_count| specified.
  options.max_count = 1;
  QueryHistory(std::string(), options, &results);
  EXPECT_FALSE(results.reached_beginning());
  QueryHistory("some", options, &results);
  EXPECT_FALSE(results.reached_beginning());

  // Test with |max_count| greater than the number of results,
  // and exactly equal to the number of results.
  options.max_count = 100;
  QueryHistory(std::string(), options, &results);
  EXPECT_TRUE(results.reached_beginning());
  options.max_count = results.size();
  QueryHistory(std::string(), options, &results);
  EXPECT_TRUE(results.reached_beginning());

  options.max_count = 100;
  QueryHistory("some", options, &results);
  EXPECT_TRUE(results.reached_beginning());
  options.max_count = results.size();
  QueryHistory("some", options, &results);
  // Since the query didn't cover the oldest visit in the database, we
  // expect false here.
  EXPECT_FALSE(results.reached_beginning());
}

// This does most of the same tests above, but searches for a FTS string that
// will match the pages in question. This will trigger a different code path.
TEST_F(HistoryQueryTest, FTS) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all of them to make sure they are there and in order. Note that
  // this query will return the starred item twice since we requested all
  // starred entries and no de-duping.
  QueryHistory("some", options, &results);
  EXPECT_EQ(3U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
  EXPECT_TRUE(NthResultIs(results, 2, 1));

  // Do a query that should only match one of them.
  QueryHistory("PAGETWO", options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));

  // Next query a time range. The beginning should be inclusive, the ending
  // should be exclusive.
  options.begin_time = test_entries[1].time;
  options.end_time = test_entries[3].time;
  QueryHistory("some", options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 1));
}

// Searches titles.
TEST_F(HistoryQueryTest, FTSTitle) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // First execute a body-only query, to ensure that it works and that that
  // version of the statement is not cached for the next query.
  options.body_only = true;
  QueryHistory("Title", options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));
  options.body_only = false;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  options.max_count = 3;
  QueryHistory("title", options, &results);
  EXPECT_EQ(3U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
  EXPECT_TRUE(NthResultIs(results, 2, 1));
}

// Tests prefix searching for Full Text Search queries.
TEST_F(HistoryQueryTest, FTSPrefix) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query with a prefix search.  Should return matches for "PAGETWO" and
  // "PAGETHREE".
  QueryHistory("PAGET", options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));
}

// Tests max_count feature for Full Text Search queries.
TEST_F(HistoryQueryTest, FTSCount) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  // Query all time but with a limit on the number of entries. We should
  // get the N most recent entries.
  options.max_count = 2;
  QueryHistory("some", options, &results);
  EXPECT_EQ(2U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 2));
  EXPECT_TRUE(NthResultIs(results, 1, 3));

  // Now query a subset of the pages and limit by N items. "FOO" should match
  // the 2nd & 3rd pages, but we should only get the 3rd one because of the one
  // page max restriction.
  options.max_count = 1;
  QueryHistory("FOO", options, &results);
  EXPECT_EQ(1U, results.size());
  EXPECT_TRUE(NthResultIs(results, 0, 3));
}

// Tests that FTS queries can find URLs when they exist only in the archived
// database. This also tests that imported URLs can be found, since we use
// AddPageWithDetails just like the importer.
TEST_F(HistoryQueryTest, FTSArchived) {
  ASSERT_TRUE(history_.get());

  URLRows urls_to_add;

  URLRow row1(GURL("http://foo.bar/"));
  row1.set_title(UTF8ToUTF16("archived title"));
  row1.set_last_visit(Time::Now() - TimeDelta::FromDays(365));
  urls_to_add.push_back(row1);

  URLRow row2(GURL("http://foo.bar/"));
  row2.set_title(UTF8ToUTF16("nonarchived title"));
  row2.set_last_visit(Time::Now());
  urls_to_add.push_back(row2);

  history_->AddPagesWithDetails(urls_to_add, history::SOURCE_BROWSED);

  QueryOptions options;
  QueryResults results;

  // Query all time. The title we get should be the one in the full text
  // database and not the most current title (since otherwise highlighting in
  // the title might be wrong).
  QueryHistory("archived", options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_TRUE(row1.url() == results[0].url());
  EXPECT_TRUE(row1.title() == results[0].title());
}

/* TODO(brettw) re-enable this. It is commented out because the current history
   code prohibits adding more than one indexed page with the same URL. When we
   have tiered history, there could be a dupe in the archived history which
   won't get picked up by the deletor and it can happen again. When this is the
   case, we should fix this test to duplicate that situation.

// Tests duplicate collapsing and not in Full Text Search situations.
TEST_F(HistoryQueryTest, FTSDupes) {
  ASSERT_TRUE(history_.get());

  QueryOptions options;
  QueryResults results;

  QueryHistory("Other", options, &results);
  EXPECT_EQ(1, results.urls().size());
  EXPECT_TRUE(NthResultIs(results, 0, 4));
}
*/

// Test iterating over pages of results.
TEST_F(HistoryQueryTest, Paging) {
  // Since results are fetched 1 and 2 at a time, entry #0 and #6 will not
  // be de-duplicated.
  int expected_results[] = { 4, 2, 3, 1, 7, 6, 5, 0 };
  TestPaging("", expected_results, arraysize(expected_results));
}

TEST_F(HistoryQueryTest, FTSPaging) {
  // Since results are fetched 1 and 2 at a time, entry #0 and #6 will not
  // be de-duplicated. Entry #4 does not contain the text "title", so it
  // shouldn't appear.
  int expected_results[] = { 2, 3, 1, 7, 6, 5 };
  TestPaging("title", expected_results, arraysize(expected_results));
}

}  // namespace history
