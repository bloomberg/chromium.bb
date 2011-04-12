// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/sql/connection.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_temp_dir.h"
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

 protected:
  // Provided for URL/VisitDatabase.
  virtual sql::Connection& GetDB() {
    return db_;
  }

 private:
  // Test setup.
  void SetUp() {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    FilePath db_file = temp_dir_.path().AppendASCII("URLTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitKeywordSearchTermsTable();
    CreateKeywordSearchTermsIndices();
  }
  void TearDown() {
    db_.Close();
  }

  ScopedTempDir temp_dir_;
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
  URLRow url_info1(GURL("http://www.google.com/"));
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
  URLRow url_info1(GURL("http://www.google.com/"));
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

TEST_F(URLDatabaseTest, EnumeratorForSignificant) {
  std::set<std::string> good_urls;
  // Add URLs which do and don't meet the criteria.
  URLRow url_no_match(GURL("http://www.url_no_match.com/"));
  EXPECT_TRUE(AddURL(url_no_match));

  std::string url_string2("http://www.url_match_visit_count.com/");
  good_urls.insert("http://www.url_match_visit_count.com/");
  URLRow url_match_visit_count(GURL("http://www.url_match_visit_count.com/"));
  url_match_visit_count.set_visit_count(kLowQualityMatchVisitLimit + 1);
  EXPECT_TRUE(AddURL(url_match_visit_count));

  good_urls.insert("http://www.url_match_typed_count.com/");
  URLRow url_match_typed_count(GURL("http://www.url_match_typed_count.com/"));
  url_match_typed_count.set_typed_count(kLowQualityMatchTypedLimit + 1);
  EXPECT_TRUE(AddURL(url_match_typed_count));

  good_urls.insert("http://www.url_match_last_visit.com/");
  URLRow url_match_last_visit(GURL("http://www.url_match_last_visit.com/"));
  url_match_last_visit.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  EXPECT_TRUE(AddURL(url_match_last_visit));

  URLRow url_no_match_last_visit(GURL(
      "http://www.url_no_match_last_visit.com/"));
  url_no_match_last_visit.set_last_visit(Time::Now() -
      TimeDelta::FromDays(kLowQualityMatchAgeLimitInDays + 1));
  EXPECT_TRUE(AddURL(url_no_match_last_visit));

  URLDatabase::URLEnumerator history_enum;
  EXPECT_TRUE(InitURLEnumeratorForSignificant(&history_enum));
  URLRow row;
  int row_count = 0;
  for (; history_enum.GetNextURL(&row); ++row_count)
    EXPECT_EQ(1U, good_urls.count(row.url().spec()));
  EXPECT_EQ(3, row_count);
}

TEST_F(URLDatabaseTest, IconMappingEnumerator) {
  const GURL url1("http://www.google.com/");
  URLRow url_info1(url1);
  url_info1.set_title(UTF8ToUTF16("Google"));
  url_info1.set_visit_count(4);
  url_info1.set_typed_count(2);
  url_info1.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info1.set_hidden(false);

  // Insert a row with favicon
  URLID url_id1 = AddURL(url_info1);
  ASSERT_TRUE(url_id1 != 0);

  FaviconID icon_id = 1;
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE urls SET favicon_id =? WHERE id=?"));

  ASSERT_TRUE(statement);

  statement.BindInt64(0, icon_id);
  statement.BindInt64(1, url_id1);
  ASSERT_TRUE(statement.Run());

  // Insert another row without favicon
  const GURL url2("http://www.google.com/no_icon");
  URLRow url_info2(url2);
  url_info2.set_title(UTF8ToUTF16("Google"));
  url_info2.set_visit_count(4);
  url_info2.set_typed_count(2);
  url_info2.set_last_visit(Time::Now() - TimeDelta::FromDays(1));
  url_info2.set_hidden(false);

  // Insert a row with favicon
  URLID url_id2 = AddURL(url_info2);
  ASSERT_TRUE(url_id2 != 0);

  IconMappingEnumerator e;
  InitIconMappingEnumeratorForEverything(&e);
  IconMapping icon_mapping;
  ASSERT_TRUE(e.GetNextIconMapping(&icon_mapping));
  ASSERT_EQ(url1, icon_mapping.page_url);
  ASSERT_EQ(icon_id, icon_mapping.icon_id);
  ASSERT_FALSE(e.GetNextIconMapping(&icon_mapping));
}

}  // namespace history
