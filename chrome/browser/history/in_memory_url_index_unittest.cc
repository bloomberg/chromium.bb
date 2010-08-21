// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <fstream>
#include <string>
#include <vector>

#include "app/sql/statement.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/browser/history/url_database.h"
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
                             public URLDatabase {
 public:
  InMemoryURLIndexTest() {}

 protected:
  // Test setup.
  void SetUp() {
    // Create and populate a working copy of the URL history database.
    FilePath history_proto_path;
    PathService::Get(chrome::DIR_TEST_DATA, &history_proto_path);
    history_proto_path = history_proto_path.Append(
        FILE_PATH_LITERAL("History"));
    history_proto_path = history_proto_path.Append(
        FILE_PATH_LITERAL("url_history_provider_test.db.txt"));
    EXPECT_TRUE(file_util::PathExists(history_proto_path));
    EXPECT_TRUE(db_.OpenInMemory());

    std::ifstream proto_file(history_proto_path.value().c_str());
    static const size_t kCommandBufferMaxSize = 2048;
    char sql_cmd_line[kCommandBufferMaxSize];
    while (!proto_file.eof()) {
      proto_file.getline(sql_cmd_line, kCommandBufferMaxSize);
      if (!proto_file.eof()) {
        // We only process lines which begin with a upper-case letter.
        // TODO(mrossetti): Can iswupper() be used here?
        if (sql_cmd_line[0] >= 'A' && sql_cmd_line[0] <= 'Z') {
          std::string sql_cmd(sql_cmd_line);
          sql::Statement sql_stmt(db_.GetUniqueStatement(sql_cmd_line));
          EXPECT_TRUE(sql_stmt.Run());
        }
      }
    }
    proto_file.close();

    // Update the last_visit_time table column
    // such that it represents a time relative to 'now'.
    sql::Statement statement(db_.GetUniqueStatement(
        "SELECT" HISTORY_URL_ROW_FIELDS "FROM urls;"));
    EXPECT_TRUE(statement);
    Time time_right_now = Time::NowFromSystemTime();
    TimeDelta  day_delta = TimeDelta::FromDays(1);
    while (statement.Step()) {
      URLRow row;
      FillURLRow(statement, &row);
      Time last_visit = time_right_now;
      for (int64 i = row.last_visit().ToInternalValue(); i > 0; --i)
        last_visit -= day_delta;
      row.set_last_visit(last_visit);
      UpdateURLRow(row.id(), row);
    }
  }

  void TearDown() {
    db_.Close();
  }

  virtual sql::Connection& GetDB() { return db_; }

  scoped_ptr<InMemoryURLIndex> url_index_;
  sql::Connection db_;
};

TEST_F(InMemoryURLIndexTest, Construction) {
  url_index_.reset(new InMemoryURLIndex);
  EXPECT_TRUE(url_index_.get());
}

TEST_F(InMemoryURLIndexTest, Initialization) {
  url_index_.reset(new InMemoryURLIndex);
  url_index_->Init(this, UTF8ToUTF16("en,ja,hi,zh"));
  // There should have been 25 of the 29 urls accepted during filtering.
  EXPECT_EQ(25U, url_index_->history_item_count_);
  // The resulting indexes should account for:
  //    37 characters
  //    88 words
  EXPECT_EQ(37U, url_index_->char_word_map_.size());
  EXPECT_EQ(88U, url_index_->word_map_.size());
}

}  // namespace history
