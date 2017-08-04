// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/autocomplete_action_predictor.h"

#include <stddef.h>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/in_memory_database.h"
#include "components/history/core/browser/url_database.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using predictors::AutocompleteActionPredictor;

namespace {

struct TestUrlInfo {
  GURL url;
  base::string16 title;
  int days_from_now;
  base::string16 user_text;
  int number_of_hits;
  int number_of_misses;
  AutocompleteActionPredictor::Action expected_action;
} test_url_db[] = {
  { GURL("http://www.testsite.com/a.html"),
    ASCIIToUTF16("Test - site - just a test"), 1,
    ASCIIToUTF16("j"), 5, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/b.html"),
    ASCIIToUTF16("Test - site - just a test"), 1,
    ASCIIToUTF16("ju"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/c.html"),
    ASCIIToUTF16("Test - site - just a test"), 5,
    ASCIIToUTF16("just"), 3, 1,
    AutocompleteActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/d.html"),
    ASCIIToUTF16("Test - site - just a test"), 5,
    ASCIIToUTF16("just"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/e.html"),
    ASCIIToUTF16("Test - site - just a test"), 8,
    ASCIIToUTF16("just"), 3, 1,
    AutocompleteActionPredictor::ACTION_PRECONNECT },
  { GURL("http://www.testsite.com/f.html"),
    ASCIIToUTF16("Test - site - just a test"), 8,
    ASCIIToUTF16("just"), 3, 0,
    AutocompleteActionPredictor::ACTION_PRERENDER },
  { GURL("http://www.testsite.com/g.html"),
    ASCIIToUTF16("Test - site - just a test"), 12,
    base::string16(), 5, 0,
    AutocompleteActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/h.html"),
    ASCIIToUTF16("Test - site - just a test"), 21,
    ASCIIToUTF16("just a test"), 2, 0,
    AutocompleteActionPredictor::ACTION_NONE },
  { GURL("http://www.testsite.com/i.html"),
    ASCIIToUTF16("Test - site - just a test"), 28,
    ASCIIToUTF16("just a test"), 2, 0,
    AutocompleteActionPredictor::ACTION_NONE }
};

}  // end namespace

namespace predictors {

class AutocompleteActionPredictorTest : public testing::Test {
 public:
  AutocompleteActionPredictorTest()
      : profile_(new TestingProfile()), predictor_(nullptr) {}

  ~AutocompleteActionPredictorTest() override {
    predictor_.reset(NULL);
    profile_.reset(NULL);
    base::RunLoop().RunUntilIdle();
  }

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kPrerenderFromOmnibox,
        switches::kPrerenderFromOmniboxSwitchValueEnabled);

    ASSERT_TRUE(profile_->CreateHistoryService(true, false));
    predictor_.reset(new AutocompleteActionPredictor(profile_.get()));
    predictor_->CreateLocalCachesFromDatabase();
    profile_->BlockUntilHistoryProcessesPendingRequests();

    ASSERT_TRUE(predictor_->initialized_);
    ASSERT_TRUE(db_cache()->empty());
    ASSERT_TRUE(db_id_cache()->empty());
  }

  void TearDown() override {
    predictor_->Shutdown();
    profile_->DestroyHistoryService();
  }

 protected:
  typedef AutocompleteActionPredictor::DBCacheKey DBCacheKey;
  typedef AutocompleteActionPredictor::DBCacheValue DBCacheValue;
  typedef AutocompleteActionPredictor::DBCacheMap DBCacheMap;
  typedef AutocompleteActionPredictor::DBIdCacheMap DBIdCacheMap;

  void AddAllRowsToHistory() {
    for (size_t i = 0; i < arraysize(test_url_db); ++i)
      ASSERT_TRUE(AddRowToHistory(test_url_db[i]));
  }

  history::URLID AddRowToHistory(const TestUrlInfo& test_row) {
    history::HistoryService* history = HistoryServiceFactory::GetForProfile(
        profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    CHECK(history);
    history::URLDatabase* url_db = history->InMemoryDatabase();
    CHECK(url_db);

    const base::Time visit_time =
        base::Time::Now() - base::TimeDelta::FromDays(
            test_row.days_from_now);

    history::URLRow row(test_row.url);
    row.set_title(test_row.title);
    row.set_last_visit(visit_time);

    return url_db->AddURL(row);
  }

  AutocompleteActionPredictorTable::Row CreateRowFromTestUrlInfo(
      const TestUrlInfo& test_row) const {
    AutocompleteActionPredictorTable::Row row;
    row.id = base::GenerateGUID();
    row.user_text = test_row.user_text;
    row.url = test_row.url;
    row.number_of_hits = test_row.number_of_hits;
    row.number_of_misses = test_row.number_of_misses;
    return row;
  }

  void AddAllRows() {
    for (size_t i = 0; i < arraysize(test_url_db); ++i)
      AddRow(test_url_db[i]);
  }

  std::string AddRow(const TestUrlInfo& test_row) {
    AutocompleteActionPredictorTable::Row row =
        CreateRowFromTestUrlInfo(test_row);
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(1, row),
        AutocompleteActionPredictorTable::Rows());

    return row.id;
  }

  void UpdateRow(const AutocompleteActionPredictorTable::Row& row) {
    AutocompleteActionPredictor::DBCacheKey key = { row.user_text, row.url };
    ASSERT_TRUE(db_cache()->find(key) != db_cache()->end());
    predictor_->AddAndUpdateRows(
        AutocompleteActionPredictorTable::Rows(),
        AutocompleteActionPredictorTable::Rows(1, row));
  }

  void DeleteAllRows() {
    predictor_->DeleteAllRows();
  }

  void DeleteRowsWithURLs(const history::URLRows& rows) {
    predictor_->DeleteRowsWithURLs(rows);
  }

  void DeleteOldIdsFromCaches(
      std::vector<AutocompleteActionPredictorTable::Row::Id>* id_list) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            profile_.get(), ServiceAccessType::EXPLICIT_ACCESS);
    ASSERT_TRUE(history_service);

    history::URLDatabase* url_db = history_service->InMemoryDatabase();
    ASSERT_TRUE(url_db);

    // Reset the predictor's |initialized_| flag for the life of this call,
    // since outside of testing this function is only supposed to be reached
    // before initialization is completed.
    base::AutoReset<bool> initialized_reset(&predictor_->initialized_, false);
    predictor_->DeleteOldIdsFromCaches(url_db, id_list);
  }

  AutocompleteActionPredictor* predictor() { return predictor_.get(); }

  DBCacheMap* db_cache() { return &predictor_->db_cache_; }
  DBIdCacheMap* db_id_cache() { return &predictor_->db_id_cache_; }

  static int maximum_days_to_keep_entry() {
    return AutocompleteActionPredictor::kMaximumDaysToKeepEntry;
  }

 private:
  content::TestBrowserThreadBundle test_browser_thread_bundle_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<AutocompleteActionPredictor> predictor_;
};


TEST_F(AutocompleteActionPredictorTest, AddRow) {
  // Add a test entry to the predictor.
  std::string guid = AddRow(test_url_db[0]);

  // Get the data back out of the cache.
  const DBCacheKey key = { test_url_db[0].user_text, test_url_db[0].url };
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());

  const DBCacheValue value = { test_url_db[0].number_of_hits,
                               test_url_db[0].number_of_misses };
  EXPECT_EQ(value.number_of_hits, it->second.number_of_hits);
  EXPECT_EQ(value.number_of_misses, it->second.number_of_misses);

  DBIdCacheMap::const_iterator id_it = db_id_cache()->find(key);
  EXPECT_TRUE(id_it != db_id_cache()->end());
  EXPECT_EQ(guid, id_it->second);
}

TEST_F(AutocompleteActionPredictorTest, UpdateRow) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(arraysize(test_url_db), db_cache()->size());
  EXPECT_EQ(arraysize(test_url_db), db_id_cache()->size());

  // Get the data back out of the cache.
  const DBCacheKey key = { test_url_db[0].user_text, test_url_db[0].url };
  DBCacheMap::const_iterator it = db_cache()->find(key);
  EXPECT_TRUE(it != db_cache()->end());

  DBIdCacheMap::const_iterator id_it = db_id_cache()->find(key);
  EXPECT_TRUE(id_it != db_id_cache()->end());

  AutocompleteActionPredictorTable::Row update_row;
  update_row.id = id_it->second;
  update_row.user_text = key.user_text;
  update_row.url = key.url;
  update_row.number_of_hits = it->second.number_of_hits + 1;
  update_row.number_of_misses = it->second.number_of_misses + 2;

  UpdateRow(update_row);

  // Get the updated version.
  DBCacheMap::const_iterator update_it = db_cache()->find(key);
  EXPECT_TRUE(update_it != db_cache()->end());

  EXPECT_EQ(update_row.number_of_hits, update_it->second.number_of_hits);
  EXPECT_EQ(update_row.number_of_misses, update_it->second.number_of_misses);

  DBIdCacheMap::const_iterator update_id_it = db_id_cache()->find(key);
  EXPECT_TRUE(update_id_it != db_id_cache()->end());

  EXPECT_EQ(id_it->second, update_id_it->second);
}

TEST_F(AutocompleteActionPredictorTest, DeleteAllRows) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(arraysize(test_url_db), db_cache()->size());
  EXPECT_EQ(arraysize(test_url_db), db_id_cache()->size());

  DeleteAllRows();

  EXPECT_TRUE(db_cache()->empty());
  EXPECT_TRUE(db_id_cache()->empty());
}

TEST_F(AutocompleteActionPredictorTest, DeleteRowsWithURLs) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  EXPECT_EQ(arraysize(test_url_db), db_cache()->size());
  EXPECT_EQ(arraysize(test_url_db), db_id_cache()->size());

  history::URLRows rows;
  for (size_t i = 0; i < 2; ++i)
    rows.push_back(history::URLRow(test_url_db[i].url));

  DeleteRowsWithURLs(rows);

  EXPECT_EQ(arraysize(test_url_db) - 2, db_cache()->size());
  EXPECT_EQ(arraysize(test_url_db) - 2, db_id_cache()->size());

  for (size_t i = 0; i < arraysize(test_url_db); ++i) {
    DBCacheKey key = { test_url_db[i].user_text, test_url_db[i].url };

    bool deleted = (i < 2);
    EXPECT_EQ(deleted, db_cache()->find(key) == db_cache()->end());
    EXPECT_EQ(deleted, db_id_cache()->find(key) == db_id_cache()->end());
  }
}

TEST_F(AutocompleteActionPredictorTest, DeleteOldIdsFromCaches) {
  std::vector<AutocompleteActionPredictorTable::Row::Id> expected;
  std::vector<AutocompleteActionPredictorTable::Row::Id> all_ids;

  for (size_t i = 0; i < arraysize(test_url_db); ++i) {
    std::string row_id = AddRow(test_url_db[i]);
    all_ids.push_back(row_id);

    bool exclude_url =
        base::StartsWith(test_url_db[i].url.path(), "/d",
                         base::CompareCase::SENSITIVE) ||
        (test_url_db[i].days_from_now > maximum_days_to_keep_entry());

    if (exclude_url)
      expected.push_back(row_id);
    else
      ASSERT_TRUE(AddRowToHistory(test_url_db[i]));
  }

  std::vector<AutocompleteActionPredictorTable::Row::Id> id_list;
  DeleteOldIdsFromCaches(&id_list);
  EXPECT_EQ(expected.size(), id_list.size());
  EXPECT_EQ(all_ids.size() - expected.size(), db_cache()->size());
  EXPECT_EQ(all_ids.size() - expected.size(), db_id_cache()->size());

  for (std::vector<AutocompleteActionPredictorTable::Row::Id>::iterator it =
       all_ids.begin();
       it != all_ids.end(); ++it) {
    bool in_expected = base::ContainsValue(expected, *it);
    bool in_list = base::ContainsValue(id_list, *it);
    EXPECT_EQ(in_expected, in_list);
  }
}

TEST_F(AutocompleteActionPredictorTest, RecommendActionURL) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::HISTORY_URL;

  for (size_t i = 0; i < arraysize(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    EXPECT_EQ(test_url_db[i].expected_action,
              predictor()->RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}

TEST_F(AutocompleteActionPredictorTest, RecommendActionSearch) {
  ASSERT_NO_FATAL_FAILURE(AddAllRows());

  AutocompleteMatch match;
  match.type = AutocompleteMatchType::SEARCH_WHAT_YOU_TYPED;

  for (size_t i = 0; i < arraysize(test_url_db); ++i) {
    match.destination_url = GURL(test_url_db[i].url);
    AutocompleteActionPredictor::Action expected_action =
        (test_url_db[i].expected_action ==
         AutocompleteActionPredictor::ACTION_PRERENDER) ?
        AutocompleteActionPredictor::ACTION_PRECONNECT :
        test_url_db[i].expected_action;
    EXPECT_EQ(expected_action,
              predictor()->RecommendAction(test_url_db[i].user_text, match))
        << "Unexpected action for " << match.destination_url;
  }
}

}  // namespace predictors
