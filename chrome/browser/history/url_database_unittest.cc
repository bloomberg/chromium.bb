// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/url_database.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace history {

namespace {

bool IsURLRowEqual(const URLRow& a,
                   const URLRow& b) {
  // TODO(brettw) when the database stores an actual Time value rather than
  // a time_t, do a reaul comparison. Instead, we have to do a more rough
  // comparison since the conversion reduces the precision.
  return a.title() == b.title() &&
      a.visit_count() == b.visit_count() &&
      a.typed_count() == b.typed_count() &&
      a.last_visit() - b.last_visit() <= TimeDelta::FromSeconds(1) &&
      a.hidden() == b.hidden();
}

}  // namespace

class URLDatabaseTest : public testing::Test,
                        public URLDatabase {
 public:
  URLDatabaseTest() {
  }

 private:
  // Test setup.
  void SetUp() {
    FilePath temp_dir;
    PathService::Get(base::DIR_TEMP, &temp_dir);
    db_file_ = temp_dir.AppendASCII("URLTest.db");

    EXPECT_TRUE(db_.Open(db_file_));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    CreateSupplimentaryURLIndices();
    InitKeywordSearchTermsTable();
    CreateKeywordSearchTermsIndices();
  }
  void TearDown() {
    db_.Close();
    file_util::Delete(db_file_, false);
  }

  // Provided for URL/VisitDatabase.
  virtual sql::Connection& GetDB() {
    return db_;
  }

  FilePath db_file_;
  sql::Connection db_;
};

// Test add and query for the URL table in the HistoryDatabase.
TEST_F(URLDatabaseTest, AddURL) {
  // First, add two URLs.
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  EXPECT_TRUE(AddURL(url_info1));

  const GURL url2("http://mail.google.com/");
  URLRow url_info2(url2);
  url_info2.set_title(UTF8ToUTF16("Google Mail"));
  url_info2.set_visit_count(3);
  url_info2.set_typed_count(0);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(2));
  url_info2.set_hidden(true);
  EXPECT_TRUE(AddURL(url_info2));

  // Query both of them.
  URLRow info;
  EXPECT_TRUE(GetRowForURL(url1, &info));
  EXPECT_TRUE(IsURLRowEqual(url_info1, info));
  URLID id2 = GetRowForURL(url2, &info);
  EXPECT_TRUE(id2);
  EXPECT_TRUE(IsURLRowEqual(url_info2, info));

  // Update the second.
  url_info2.set_title(UTF8ToUTF16("Google Mail Too"));
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(1);
  url_info2.set_typed_count(91011);
  url_info2.set_hidden(false);
  EXPECT_TRUE(UpdateURLRow(id2, url_info2));

  // Make sure it got updated.
  URLRow info2;
  EXPECT_TRUE(GetRowForURL(url2, &info2));
  EXPECT_TRUE(IsURLRowEqual(url_info2, info2));

  // Query a nonexistent URL.
  EXPECT_EQ(0, GetRowForURL(GURL("http://news.google.com/"), &info));

  // Delete all urls in the domain.
  // TODO(acw): test the new url based delete domain
  // EXPECT_TRUE(db.DeleteDomain(kDomainID));

  // Make sure the urls have been properly removed.
  // TODO(acw): commented out because remove no longer works.
  // EXPECT_TRUE(db.GetURLInfo(url1, NULL) == NULL);
  // EXPECT_TRUE(db.GetURLInfo(url2, NULL) == NULL);
}

// Tests adding, querying and deleting keyword visits.
TEST_F(URLDatabaseTest, KeywordSearchTermVisit) {
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_TRUE(url_id != 0);

  // Add a keyword visit.
  TemplateURLID keyword_id = 100;
  string16 keyword = UTF8ToUTF16("visit");
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, keyword_id, keyword));

  // Make sure we get it back.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(keyword_id, keyword, 10, &matches);
  ASSERT_EQ(1U, matches.size());
  ASSERT_EQ(keyword, matches[0].term);

  KeywordSearchTermRow keyword_search_term_row;
  ASSERT_TRUE(GetKeywordSearchTermRow(url_id, &keyword_search_term_row));
  EXPECT_EQ(keyword_id, keyword_search_term_row.keyword_id);
  EXPECT_EQ(url_id, keyword_search_term_row.url_id);
  EXPECT_EQ(keyword, keyword_search_term_row.term);

  // Delete the keyword visit.
  DeleteAllSearchTermsForKeyword(keyword_id);

  // Make sure we don't get it back when querying.
  matches.clear();
  GetMostRecentKeywordSearchTerms(keyword_id, keyword, 10, &matches);
  ASSERT_EQ(0U, matches.size());

  ASSERT_FALSE(GetKeywordSearchTermRow(url_id, &keyword_search_term_row));
}

// Make sure deleting a URL also deletes a keyword visit.
TEST_F(URLDatabaseTest, DeleteURLDeletesKeywordSearchTermVisit) {
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);
  URLID url_id = AddURL(url_info1);
  ASSERT_TRUE(url_id != 0);

  // Add a keyword visit.
  ASSERT_TRUE(SetKeywordSearchTermsForURL(url_id, 1, UTF8ToUTF16("visit")));

  // Delete the url.
  ASSERT_TRUE(DeleteURLRow(url_id));

  // Make sure the keyword visit was deleted.
  std::vector<KeywordSearchTermVisit> matches;
  GetMostRecentKeywordSearchTerms(1, UTF8ToUTF16("visit"), 10, &matches);
  ASSERT_EQ(0U, matches.size());
}

}  // namespace history
