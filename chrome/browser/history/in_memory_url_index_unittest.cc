// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <fstream>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

// The test version of the history url database table ('url') is contained in
// a database file created from a text file('url_history_provider_test.db.txt').
// The only difference between this table and a live 'urls' table from a
// profile is that the last_visit_time column in the test table contains a
// number specifying the number of days relative to 'today' to which the
// absolute time should be set during the test setup stage.
//
// The format of the test database text file is of a SQLite .dump file.
// Note that only lines whose first character is an upper-case letter are
// processed when creating the test database.

using base::Time;
using base::TimeDelta;

namespace history {

class InMemoryURLIndexTest : public testing::Test,
                             public InMemoryDatabase {
 public:
  InMemoryURLIndexTest() { InitFromScratch(); }

 protected:
  // Test setup.
  virtual void SetUp() {
    // Create and populate a working copy of the URL history database.
    FilePath history_proto_path;
    PathService::Get(chrome::DIR_TEST_DATA, &history_proto_path);
    history_proto_path = history_proto_path.Append(
        FILE_PATH_LITERAL("History"));
    history_proto_path = history_proto_path.Append(
        FILE_PATH_LITERAL("url_history_provider_test.db.txt"));
    EXPECT_TRUE(file_util::PathExists(history_proto_path));

    std::ifstream proto_file(history_proto_path.value().c_str());
    static const size_t kCommandBufferMaxSize = 2048;
    char sql_cmd_line[kCommandBufferMaxSize];

    sql::Connection& db(GetDB());
    {
      sql::Transaction transaction(&db);
      transaction.Begin();
      while (!proto_file.eof()) {
        proto_file.getline(sql_cmd_line, kCommandBufferMaxSize);
        if (!proto_file.eof()) {
          // We only process lines which begin with a upper-case letter.
          // TODO(mrossetti): Can iswupper() be used here?
          if (sql_cmd_line[0] >= 'A' && sql_cmd_line[0] <= 'Z') {
            std::string sql_cmd(sql_cmd_line);
            sql::Statement sql_stmt(db.GetUniqueStatement(sql_cmd_line));
            EXPECT_TRUE(sql_stmt.Run());
          }
        }
      }
      transaction.Commit();
    }
    proto_file.close();

    // Update the last_visit_time table column
    // such that it represents a time relative to 'now'.
    sql::Statement statement(db.GetUniqueStatement(
        "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls;"));
    EXPECT_TRUE(statement);
    Time time_right_now = Time::NowFromSystemTime();
    TimeDelta  day_delta = TimeDelta::FromDays(1);
    {
      sql::Transaction transaction(&db);
      transaction.Begin();
      while (statement.Step()) {
        URLRow row;
        FillURLRow(statement, &row);
        Time last_visit = time_right_now;
        for (int64 i = row.last_visit().ToInternalValue(); i > 0; --i)
          last_visit -= day_delta;
        row.set_last_visit(last_visit);
        UpdateURLRow(row.id(), row);
      }
      transaction.Commit();
    }
  }

  scoped_ptr<InMemoryURLIndex> url_index_;
};

TEST_F(InMemoryURLIndexTest, Construction) {
  url_index_.reset(new InMemoryURLIndex);
  EXPECT_TRUE(url_index_.get());
}

// TODO(mrossetti): Write python script to calculate the validation criteria.
TEST_F(InMemoryURLIndexTest, Initialization) {
  // Verify that the database contains the expected number of items, which
  // is the pre-filtered count, i.e. all of the items.
  sql::Statement statement(GetDB().GetUniqueStatement("SELECT * FROM urls;"));
  EXPECT_TRUE(statement);
  uint64 row_count = 0;
  while (statement.Step()) ++row_count;
  EXPECT_EQ(33U, row_count);
  url_index_.reset(new InMemoryURLIndex);
  url_index_->Init(this, "en,ja,hi,zh");

  // There should have been 27 of the 31 urls accepted during filtering.
  EXPECT_EQ(28U, url_index_->history_item_count_);

  // history_info_map_ should have the same number of items as were filtered.
  EXPECT_EQ(28U, url_index_->history_info_map_.size());

  // The resulting indexes should account for:
  //    37 characters
  //    88 words
  EXPECT_EQ(37U, url_index_->char_word_map_.size());
  EXPECT_EQ(91U, url_index_->word_map_.size());
}

TEST_F(InMemoryURLIndexTest, Retrieval) {
  url_index_.reset(new InMemoryURLIndex);
  std::string languages("en,ja,hi,zh");
  url_index_->Init(this, languages);
  InMemoryURLIndex::String16Vector terms;
  // The term will be lowercased by the search.

  // See if a very specific term gives a single result.
  string16 term = ASCIIToUTF16("DrudgeReport");
  terms.push_back(term);
  ScoredHistoryMatches matches = url_index_->HistoryItemsForTerms(terms);
  EXPECT_EQ(1U, matches.size());

  // Search which should result in multiple results.
  terms.clear();
  term = ASCIIToUTF16("drudge");
  terms.push_back(term);
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(2U, matches.size());
  // The results should be in descending score order.
  EXPECT_GT(matches[0].raw_score, matches[1].raw_score);

  // Search which should result in nearly perfect result.
  terms.clear();
  term = ASCIIToUTF16("http");
  terms.push_back(term);
  term = ASCIIToUTF16("NearlyPerfectResult");
  terms.push_back(term);
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());
  // The results should have a very high score.
  EXPECT_GT(matches[0].raw_score, 900);

  // Search which should result in very poor result.
  terms.clear();
  term = ASCIIToUTF16("z");
  terms.push_back(term);
  term = ASCIIToUTF16("y");
  terms.push_back(term);
  term = ASCIIToUTF16("x");
  terms.push_back(term);
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());
  // The results should have a poor score.
  EXPECT_LT(matches[0].raw_score, 200);
}

}  // namespace history
