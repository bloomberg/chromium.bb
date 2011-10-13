// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <fstream>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/in_memory_url_index.h"
#include "chrome/common/chrome_paths.h"
#include "sql/connection.h"
#include "sql/statement.h"
#include "sql/transaction.h"
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

namespace history {

class InMemoryURLIndexTest : public testing::Test,
                             public InMemoryDatabase {
 public:
  InMemoryURLIndexTest() { InitFromScratch(); }

 protected:
  // Test setup.
  virtual void SetUp();

  // Allows the database containing the test data to be customized by
  // subclasses.
  virtual FilePath::StringType TestDBName() const;

  // Convenience function to create a URLRow with basic data for |url|, |title|,
  // |visit_count|, and |typed_count|. |last_visit_ago| gives the number of
  // days from now to set the URL's last_visit.
  URLRow MakeURLRow(const char* url,
                    const char* title,
                    int visit_count,
                    int last_visit_ago,
                    int typed_count);

  // Convenience functions for easily creating vectors of search terms.
  InMemoryURLIndex::String16Vector Make1Term(const char* term) const;
  InMemoryURLIndex::String16Vector Make2Terms(const char* term_1,
                                              const char* term_2) const;

  // Validates that the given |term| is contained in |cache| and that it is
  // marked as in-use.
  void CheckTerm(const InMemoryURLIndex::SearchTermCacheMap& cache,
                 string16 term) const;

  scoped_ptr<InMemoryURLIndex> url_index_;
};

void InMemoryURLIndexTest::SetUp() {
  // Create and populate a working copy of the URL history database.
  FilePath history_proto_path;
  PathService::Get(chrome::DIR_TEST_DATA, &history_proto_path);
  history_proto_path = history_proto_path.Append(
      FILE_PATH_LITERAL("History"));
  history_proto_path = history_proto_path.Append(TestDBName());
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
  base::Time time_right_now = base::Time::NowFromSystemTime();
  base::TimeDelta day_delta = base::TimeDelta::FromDays(1);
  {
    sql::Transaction transaction(&db);
    transaction.Begin();
    while (statement.Step()) {
      URLRow row;
      FillURLRow(statement, &row);
      base::Time last_visit = time_right_now;
      for (int64 i = row.last_visit().ToInternalValue(); i > 0; --i)
        last_visit -= day_delta;
      row.set_last_visit(last_visit);
      UpdateURLRow(row.id(), row);
    }
    transaction.Commit();
  }
}

FilePath::StringType InMemoryURLIndexTest::TestDBName() const {
    return FILE_PATH_LITERAL("url_history_provider_test.db.txt");
}

URLRow InMemoryURLIndexTest::MakeURLRow(const char* url,
                  const char* title,
                  int visit_count,
                  int last_visit_ago,
                  int typed_count) {
  URLRow row(GURL(url), 0);
  row.set_title(UTF8ToUTF16(title));
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(base::Time::NowFromSystemTime() -
                     base::TimeDelta::FromDays(last_visit_ago));
  return row;
}

InMemoryURLIndex::String16Vector InMemoryURLIndexTest::Make1Term(
    const char* term) const {
  InMemoryURLIndex::String16Vector terms;
  terms.push_back(UTF8ToUTF16(term));
  return terms;
}

InMemoryURLIndex::String16Vector InMemoryURLIndexTest::Make2Terms(
    const char* term_1,
    const char* term_2) const {
  InMemoryURLIndex::String16Vector terms;
  terms.push_back(UTF8ToUTF16(term_1));
  terms.push_back(UTF8ToUTF16(term_2));
  return terms;
}

void InMemoryURLIndexTest::CheckTerm(
    const InMemoryURLIndex::SearchTermCacheMap& cache,
    string16 term) const {
  InMemoryURLIndex::SearchTermCacheMap::const_iterator cache_iter(
      cache.find(term));
  ASSERT_NE(cache.end(), cache_iter)
      << "Cache does not contain '" << term << "' but should.";
  InMemoryURLIndex::SearchTermCacheItem cache_item = cache_iter->second;
  EXPECT_TRUE(cache_item.used_)
      << "Cache item '" << term << "' should be marked as being in use.";
}

class LimitedInMemoryURLIndexTest : public InMemoryURLIndexTest {
 protected:
  FilePath::StringType TestDBName() const;
};

FilePath::StringType LimitedInMemoryURLIndexTest::TestDBName() const {
  return FILE_PATH_LITERAL("url_history_provider_test_limited.db.txt");
}

class ExpandedInMemoryURLIndexTest : public InMemoryURLIndexTest {
 protected:
  virtual void SetUp();
};

void ExpandedInMemoryURLIndexTest::SetUp() {
  InMemoryURLIndexTest::SetUp();
  // Add 600 more history items.
  // NOTE: Keep the string length constant at least the length of the format
  // string plus 5 to account for a 3 digit number and terminator.
  char url_format[] = "http://www.google.com/%d";
  const size_t kMaxLen = arraysize(url_format) + 5;
  char url_string[kMaxLen + 1];
  for (int i = 0; i < 600; ++i) {
    base::snprintf(url_string, kMaxLen, url_format, i);
    URLRow row(MakeURLRow(url_string, "Google Search", 20, 0, 20));
    AddURL(row);
  }
}

TEST_F(InMemoryURLIndexTest, Construction) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  EXPECT_TRUE(url_index_.get());
}

TEST_F(LimitedInMemoryURLIndexTest, Initialization) {
  // Verify that the database contains the expected number of items, which
  // is the pre-filtered count, i.e. all of the items.
  sql::Statement statement(GetDB().GetUniqueStatement("SELECT * FROM urls;"));
  EXPECT_TRUE(statement);
  uint64 row_count = 0;
  while (statement.Step()) ++row_count;
  EXPECT_EQ(1U, row_count);
  url_index_.reset(new InMemoryURLIndex);
  url_index_->Init(this, "en,ja,hi,zh");
  EXPECT_EQ(1, url_index_->history_item_count_);

  // history_info_map_ should have the same number of items as were filtered.
  EXPECT_EQ(1U, url_index_->history_info_map_.size());
  EXPECT_EQ(35U, url_index_->char_word_map_.size());
  EXPECT_EQ(17U, url_index_->word_map_.size());
}

TEST_F(InMemoryURLIndexTest, Retrieval) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");
  // The term will be lowercased by the search.

  // See if a very specific term gives a single result.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(Make1Term("DrudgeReport"));
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16("DRUDGE REPORT 2010"), matches[0].url_info.title());

  // Search which should result in multiple results.
  matches = url_index_->HistoryItemsForTerms(Make1Term("drudge"));
  ASSERT_EQ(2U, matches.size());
  // The results should be in descending score order.
  EXPECT_GE(matches[0].raw_score, matches[1].raw_score);

  // Search which should result in nearly perfect result.
  matches = url_index_->HistoryItemsForTerms(Make2Terms("https",
                                                        "NearlyPerfectResult"));
  ASSERT_EQ(1U, matches.size());
  // The results should have a very high score.
  EXPECT_GT(matches[0].raw_score, 900);
  EXPECT_EQ(32, matches[0].url_info.id());
  EXPECT_EQ("https://nearlyperfectresult.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Perfect Search Result"),
            matches[0].url_info.title());

  // Search which should result in very poor result.
  InMemoryURLIndex::String16Vector terms;
  terms.push_back(ASCIIToUTF16("z"));
  terms.push_back(ASCIIToUTF16("y"));
  terms.push_back(ASCIIToUTF16("x"));
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());
  // The results should have a poor score.
  EXPECT_LT(matches[0].raw_score, 500);
  EXPECT_EQ(33, matches[0].url_info.id());
  EXPECT_EQ("http://quiteuselesssearchresultxyz.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Useless Search Result"),
            matches[0].url_info.title());

  // Search which will match at the end of an URL with encoded characters.
  matches = url_index_->HistoryItemsForTerms(Make1Term("ice"));
  ASSERT_EQ(1U, matches.size());
}

TEST_F(ExpandedInMemoryURLIndexTest, ShortCircuit) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");

  // A search for 'w' should short-circuit and not return any matches.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(Make1Term("w"));
  EXPECT_TRUE(matches.empty());

  // A search for 'working' should not short-circuit.
  matches = url_index_->HistoryItemsForTerms(Make1Term("working"));
  EXPECT_EQ(1U, matches.size());
}

TEST_F(InMemoryURLIndexTest, TitleSearch) {
  url_index_.reset(new InMemoryURLIndex());
  url_index_->Init(this, "en,ja,hi,zh");
  // Signal if someone has changed the test DB.
  EXPECT_EQ(27U, url_index_->history_info_map_.size());
  InMemoryURLIndex::String16Vector terms;

  // Ensure title is being searched.
  terms.push_back(ASCIIToUTF16("MORTGAGE"));
  terms.push_back(ASCIIToUTF16("RATE"));
  terms.push_back(ASCIIToUTF16("DROPS"));
  ScoredHistoryMatches matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(1, matches[0].url_info.id());
  EXPECT_EQ("http://www.reuters.com/article/idUSN0839880620100708",
            matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16(
      "UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters"),
      matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, NonUniqueTermCharacterSets) {
  url_index_.reset(new InMemoryURLIndex());
  url_index_->Init(this, "en,ja,hi,zh");

  // The presence of duplicate characters should succeed. Exercise by cycling
  // through a string with several duplicate characters.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(Make1Term("ABRA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
  EXPECT_EQ("http://www.ddj.com/windows/184416623",
            matches[0].url_info.url().spec());

  matches = url_index_->HistoryItemsForTerms(Make1Term("ABRACAD"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(Make1Term("ABRACADABRA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(Make1Term("ABRACADABR"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(Make1Term("ABRACA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
}

TEST_F(InMemoryURLIndexTest, StaticFunctions) {
  // Test WordVectorFromString16
  string16 string_a(ASCIIToUTF16("http://www.google.com/ frammy the brammy"));
  InMemoryURLIndex::String16Vector string_vec =
      InMemoryURLIndex::WordVectorFromString16(string_a, false);
  ASSERT_EQ(7U, string_vec.size());
  // See if we got the words we expected.
  EXPECT_EQ(UTF8ToUTF16("http"), string_vec[0]);
  EXPECT_EQ(UTF8ToUTF16("www"), string_vec[1]);
  EXPECT_EQ(UTF8ToUTF16("google"), string_vec[2]);
  EXPECT_EQ(UTF8ToUTF16("com"), string_vec[3]);
  EXPECT_EQ(UTF8ToUTF16("frammy"), string_vec[4]);
  EXPECT_EQ(UTF8ToUTF16("the"), string_vec[5]);
  EXPECT_EQ(UTF8ToUTF16("brammy"), string_vec[6]);

  string_vec = InMemoryURLIndex::WordVectorFromString16(string_a, true);
  ASSERT_EQ(5U, string_vec.size());
  EXPECT_EQ(UTF8ToUTF16("http://"), string_vec[0]);
  EXPECT_EQ(UTF8ToUTF16("www.google.com/"), string_vec[1]);
  EXPECT_EQ(UTF8ToUTF16("frammy"), string_vec[2]);
  EXPECT_EQ(UTF8ToUTF16("the"), string_vec[3]);
  EXPECT_EQ(UTF8ToUTF16("brammy"), string_vec[4]);

  // Test WordSetFromString16
  string16 string_b(ASCIIToUTF16(
      "http://web.google.com/search Google Web Search"));
  InMemoryURLIndex::String16Set string_set =
      InMemoryURLIndex::WordSetFromString16(string_b);
  EXPECT_EQ(5U, string_set.size());
  // See if we got the words we expected.
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("com")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("google")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("http")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("search")) != string_set.end());
  EXPECT_TRUE(string_set.find(UTF8ToUTF16("web")) != string_set.end());

  // Test SortAndDeoverlap
  TermMatches matches_a;
  matches_a.push_back(TermMatch(1, 13, 10));
  matches_a.push_back(TermMatch(2, 23, 10));
  matches_a.push_back(TermMatch(3, 3, 10));
  matches_a.push_back(TermMatch(4, 40, 5));
  TermMatches matches_b = InMemoryURLIndex::SortAndDeoverlap(matches_a);
  // Nothing should have been eliminated.
  EXPECT_EQ(matches_a.size(), matches_b.size());
  // The order should now be 3, 1, 2, 4.
  EXPECT_EQ(3, matches_b[0].term_num);
  EXPECT_EQ(1, matches_b[1].term_num);
  EXPECT_EQ(2, matches_b[2].term_num);
  EXPECT_EQ(4, matches_b[3].term_num);
  matches_a.push_back(TermMatch(5, 18, 10));
  matches_a.push_back(TermMatch(6, 38, 5));
  matches_b = InMemoryURLIndex::SortAndDeoverlap(matches_a);
  // Two matches should have been eliminated.
  EXPECT_EQ(matches_a.size() - 2, matches_b.size());
  // The order should now be 3, 1, 2, 6.
  EXPECT_EQ(3, matches_b[0].term_num);
  EXPECT_EQ(1, matches_b[1].term_num);
  EXPECT_EQ(2, matches_b[2].term_num);
  EXPECT_EQ(6, matches_b[3].term_num);

  // Test MatchTermInString
  TermMatches matches_c = InMemoryURLIndex::MatchTermInString(
      UTF8ToUTF16("x"), UTF8ToUTF16("axbxcxdxex fxgx/hxixjx.kx"), 123);
  ASSERT_EQ(11U, matches_c.size());
  const size_t expected_offsets[] = { 1, 3, 5, 7, 9, 12, 14, 17, 19, 21, 24 };
  for (int i = 0; i < 11; ++i)
    EXPECT_EQ(expected_offsets[i], matches_c[i].offset);
}

TEST_F(InMemoryURLIndexTest, OffsetsAndTermMatches) {
  // Test OffsetsFromTermMatches
  history::TermMatches matches_a;
  matches_a.push_back(history::TermMatch(1, 1, 2));
  matches_a.push_back(history::TermMatch(2, 4, 3));
  matches_a.push_back(history::TermMatch(3, 9, 1));
  matches_a.push_back(history::TermMatch(3, 10, 1));
  matches_a.push_back(history::TermMatch(4, 14, 5));
  std::vector<size_t> offsets =
      InMemoryURLIndex::OffsetsFromTermMatches(matches_a);
  const size_t expected_offsets_a[] = {1, 4, 9, 10, 14};
  ASSERT_EQ(offsets.size(), arraysize(expected_offsets_a));
  for (size_t i = 0; i < offsets.size(); ++i)
    EXPECT_EQ(expected_offsets_a[i], offsets[i]);

  // Test ReplaceOffsetsInTermMatches
  offsets[2] = string16::npos;
  history::TermMatches matches_b =
      InMemoryURLIndex::ReplaceOffsetsInTermMatches(matches_a, offsets);
  const size_t expected_offsets_b[] = {1, 4, 10, 14};
  ASSERT_EQ(arraysize(expected_offsets_b), matches_b.size());
  for (size_t i = 0; i < matches_b.size(); ++i)
    EXPECT_EQ(expected_offsets_b[i], matches_b[i].offset);
}

TEST_F(InMemoryURLIndexTest, TypedCharacterCaching) {
  // Verify that match results for previously typed characters are retained
  // (in the term_char_word_set_cache_) and reused, if possible, in future
  // autocompletes.
  typedef InMemoryURLIndex::SearchTermCacheMap::iterator CacheIter;
  typedef InMemoryURLIndex::SearchTermCacheItem CacheItem;

  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");

  InMemoryURLIndex::SearchTermCacheMap& cache(url_index_->search_term_cache_);

  // The cache should be empty at this point.
  EXPECT_EQ(0U, cache.size());

  // Now simulate typing search terms into the omnibox and check the state of
  // the cache as each item is 'typed'.

  // Simulate typing "r" giving "r" in the simulated omnibox. The results for
  // 'r' will be not cached because it is only 1 character long.
  InMemoryURLIndex::String16Vector terms;
  string16 term_r = ASCIIToUTF16("r");
  terms.push_back(term_r);
  url_index_->HistoryItemsForTerms(terms);
  EXPECT_EQ(0U, cache.size());

  // Simulate typing "re" giving "r re" in the simulated omnibox.
  string16 term_re = ASCIIToUTF16("re");
  terms.push_back(term_re);
  // 're' should be cached at this point but not 'r' as it is a single
  // character.
  ASSERT_EQ(2U, terms.size());
  url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, term_re);

  // Simulate typing "reco" giving "r re reco" in the simulated omnibox.
  string16 term_reco = ASCIIToUTF16("reco");
  terms.push_back(term_reco);
  // 're' and 'reco' should be cached at this point but not 'r' as it is a
  // single character.
  url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, term_re);
  CheckTerm(cache, term_reco);

  terms.clear();  // Simulate pressing <ESC>.

  // Simulate typing "mort".
  string16 term_mort = ASCIIToUTF16("mort");
  terms.push_back(term_mort);
  // Since we now have only one search term, the cached results for 're' and
  // 'reco' should be purged, giving us only 1 item in the cache (for 'mort').
  url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, term_mort);

  // Simulate typing "reco" giving "mort reco" in the simulated omnibox.
  terms.push_back(term_reco);
  url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, term_mort);
  CheckTerm(cache, term_reco);

  // Simulate a <DELETE> by removing the 'reco' and adding back the 'rec'.
  terms.resize(terms.size() - 1);
  string16 term_rec = ASCIIToUTF16("rec");
  terms.push_back(term_rec);
  url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, term_mort);
  CheckTerm(cache, term_rec);
}

TEST_F(InMemoryURLIndexTest, Scoring) {
  URLRow row_a(MakeURLRow("http://abcdef", "fedcba", 3, 30, 1));
  // Test scores based on position.
  ScoredHistoryMatch scored_a(
      InMemoryURLIndex::ScoredMatchForURL(row_a, Make1Term("abc")));
  ScoredHistoryMatch scored_b(
      InMemoryURLIndex::ScoredMatchForURL(row_a, Make1Term("bcd")));
  EXPECT_GT(scored_a.raw_score, scored_b.raw_score);
  // Test scores based on length.
  ScoredHistoryMatch scored_c(
      InMemoryURLIndex::ScoredMatchForURL(row_a, Make1Term("abcd")));
  EXPECT_LT(scored_a.raw_score, scored_c.raw_score);
  // Test scores based on order.
  ScoredHistoryMatch scored_d(
      InMemoryURLIndex::ScoredMatchForURL(row_a, Make2Terms("abc", "def")));
  ScoredHistoryMatch scored_e(
      InMemoryURLIndex::ScoredMatchForURL(row_a, Make2Terms("def", "abc")));
  EXPECT_GT(scored_d.raw_score, scored_e.raw_score);
  // Test scores based on visit_count.
  URLRow row_b(MakeURLRow("http://abcdef", "fedcba", 10, 30, 1));
  ScoredHistoryMatch scored_f(
      InMemoryURLIndex::ScoredMatchForURL(row_b, Make1Term("abc")));
  EXPECT_GT(scored_f.raw_score, scored_a.raw_score);
  // Test scores based on last_visit.
  URLRow row_c(MakeURLRow("http://abcdef", "fedcba", 3, 10, 1));
  ScoredHistoryMatch scored_g(
      InMemoryURLIndex::ScoredMatchForURL(row_c, Make1Term("abc")));
  EXPECT_GT(scored_g.raw_score, scored_a.raw_score);
  // Test scores based on typed_count.
  URLRow row_d(MakeURLRow("http://abcdef", "fedcba", 3, 30, 10));
  ScoredHistoryMatch scored_h(
      InMemoryURLIndex::ScoredMatchForURL(row_d, Make1Term("abc")));
  EXPECT_GT(scored_h.raw_score, scored_a.raw_score);
}

TEST_F(InMemoryURLIndexTest, AddNewRows) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");
  InMemoryURLIndex::String16Vector terms;

  // Verify that the row we're going to add does not already exist.
  URLID new_row_id = 87654321;
  // Newly created URLRows get a last_visit time of 'right now' so it should
  // qualify as a quick result candidate.
  terms.push_back(ASCIIToUTF16("brokeandalone"));
  EXPECT_TRUE(url_index_->HistoryItemsForTerms(terms).empty());

  // Add a new row.
  URLRow new_row(GURL("http://www.brokeandaloneinmanitoba.com/"), new_row_id);
  new_row.set_last_visit(base::Time::Now());
  url_index_->UpdateURL(new_row_id, new_row);

  // Verify that we can retrieve it.
  EXPECT_EQ(1U, url_index_->HistoryItemsForTerms(terms).size());

  // Add it again just to be sure that is harmless.
  url_index_->UpdateURL(new_row_id, new_row);
  EXPECT_EQ(1U, url_index_->HistoryItemsForTerms(terms).size());
}

TEST_F(InMemoryURLIndexTest, DeleteRows) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");
  InMemoryURLIndex::String16Vector terms;

  // Make sure we actually get an existing result.
  terms.push_back(ASCIIToUTF16("DrudgeReport"));
  ScoredHistoryMatches matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());

  // Determine the row id for that result, delete that id, then search again.
  url_index_->DeleteURL(matches[0].url_info.id());
  EXPECT_TRUE(url_index_->HistoryItemsForTerms(terms).empty());
}

TEST_F(InMemoryURLIndexTest, WhitelistedURLs) {
  struct TestData {
    const std::string url_spec;
    const bool expected_is_whitelisted;
  } data[] = {
    // URLs with whitelisted schemes.
    { "about:histograms", true },
    { "chrome://settings", true },
    { "file://localhost/Users/joeschmoe/sekrets", true },
    { "ftp://public.mycompany.com/myfile.txt", true },
    { "http://www.google.com/translate", true },
    { "https://www.gmail.com/", true },
    { "mailto:support@google.com", true },
    // URLs with unacceptable schemes.
    { "aaa://www.dummyhost.com;frammy", false },
    { "aaas://www.dummyhost.com;frammy", false },
    { "acap://suzie@somebody.com", false },
    { "cap://cal.example.com/Company/Holidays", false },
    { "cid:foo4*foo1@bar.net", false },
    { "crid://example.com/foobar", false },
    { "data:image/png;base64,iVBORw0KGgoAAAANSUhE=", false },
    { "dict://dict.org/d:shortcake:", false },
    { "dns://192.168.1.1/ftp.example.org?type=A", false },
    { "fax:+358.555.1234567", false },
    { "geo:13.4125,103.8667", false },
    { "go:Mercedes%20Benz", false },
    { "gopher://farnsworth.ca:666/gopher", false },
    { "h323:farmer-john;sixpence", false },
    { "iax:johnQ@example.com/12022561414", false },
    { "icap://icap.net/service?mode=translate&lang=french", false },
    { "im:fred@example.com", false },
    { "imap://michael@minbari.org/users.*", false },
    { "info:ddc/22/eng//004.678", false },
    { "ipp://example.com/printer/fox", false },
    { "iris:dreg1//example.com/local/myhosts", false },
    { "iris.beep:dreg1//example.com/local/myhosts", false },
    { "iris.lws:dreg1//example.com/local/myhosts", false },
    { "iris.xpc:dreg1//example.com/local/myhosts", false },
    { "iris.xpcs:dreg1//example.com/local/myhosts", false },
    { "ldap://ldap.itd.umich.edu/o=University%20of%20Michigan,c=US", false },
    { "mid:foo4%25foo1@bar.net", false },
    { "modem:+3585551234567;type=v32b?7e1;type=v110", false },
    { "msrp://atlanta.example.com:7654/jshA7weztas;tcp", false },
    { "msrps://atlanta.example.com:7654/jshA7weztas;tcp", false },
    { "news:colorectal.info.banned", false },
    { "nfs://server/d/e/f", false },
    { "nntp://www.example.com:6543/info.comp.lies/1234", false },
    { "pop://rg;AUTH=+APOP@mail.mycompany.com:8110", false },
    { "pres:fred@example.com", false },
    { "prospero://host.dom//pros/name", false },
    { "rsync://syler@lost.com/Source", false },
    { "rtsp://media.example.com:554/twister/audiotrack", false },
    { "service:acap://some.where.net;authentication=KERBEROSV4", false },
    { "shttp://www.terces.com/secret", false },
    { "sieve://example.com//script", false },
    { "sip:+1-212-555-1212:1234@gateway.com;user=phone", false },
    { "sips:+1-212-555-1212:1234@gateway.com;user=phone", false },
    { "sms:+15105551212?body=hello%20there", false },
    { "snmp://tester5@example.com:8161/bridge1;800002b804616263", false },
    { "soap.beep://stockquoteserver.example.com/StockQuote", false },
    { "soap.beeps://stockquoteserver.example.com/StockQuote", false },
    { "tag:blogger.com,1999:blog-555", false },
    { "tel:+358-555-1234567;postd=pp22", false },
    { "telnet://mayor_margie:one2rule4All@www.mycity.com:6789/", false },
    { "tftp://example.com/mystartupfile", false },
    { "tip://123.123.123.123/?urn:xopen:xid", false },
    { "tv:nbc.com", false },
    { "urn:foo:A123,456", false },
    { "vemmi://zeus.mctel.fr/demo", false },
    { "wais://www.mydomain.net:8765/mydatabase", false },
    { "xmpp:node@example.com", false },
    { "xmpp://guest@example.com", false },
  };
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL(
      "/flammmy/frammy/"))));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    GURL url(data[i].url_spec);
    EXPECT_EQ(data[i].expected_is_whitelisted,
              url_index_->URLSchemeIsWhitelisted(url));
  }
}

TEST_F(InMemoryURLIndexTest, CacheFilePath) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL(
      "/flammmy/frammy/"))));
  FilePath full_file_path;
  url_index_->GetCacheFilePath(&full_file_path);
  std::vector<FilePath::StringType> expected_parts;
  FilePath(FILE_PATH_LITERAL("/flammmy/frammy/History Provider Cache")).
      GetComponents(&expected_parts);
  std::vector<FilePath::StringType> actual_parts;
  full_file_path.GetComponents(&actual_parts);
  ASSERT_EQ(expected_parts.size(), actual_parts.size());
  size_t count = expected_parts.size();
  for (size_t i = 0; i < count; ++i)
    EXPECT_EQ(expected_parts[i], actual_parts[i]);
}

TEST_F(InMemoryURLIndexTest, CacheSaveRestore) {
  // Save the cache to a protobuf, restore it, and compare the results.
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  InMemoryURLIndex& url_index(*(url_index_.get()));
  url_index.Init(this, "en,ja,hi,zh");
  in_memory_url_index::InMemoryURLIndexCacheItem index_cache;
  url_index.SavePrivateData(&index_cache);

  // Capture our private data so we can later compare for equality.
  int history_item_count(url_index.history_item_count_);
  InMemoryURLIndex::String16Vector word_list(url_index.word_list_);
  InMemoryURLIndex::WordMap word_map(url_index.word_map_);
  InMemoryURLIndex::CharWordIDMap char_word_map(url_index.char_word_map_);
  InMemoryURLIndex::WordIDHistoryMap word_id_history_map(
      url_index.word_id_history_map_);
  InMemoryURLIndex::HistoryInfoMap history_info_map(
      url_index.history_info_map_);

  // Prove that there is really something there.
  EXPECT_GT(url_index.history_item_count_, 0);
  EXPECT_FALSE(url_index.word_list_.empty());
  EXPECT_FALSE(url_index.word_map_.empty());
  EXPECT_FALSE(url_index.char_word_map_.empty());
  EXPECT_FALSE(url_index.word_id_history_map_.empty());
  EXPECT_FALSE(url_index.history_info_map_.empty());

  // Clear and then prove it's clear.
  url_index.ClearPrivateData();
  EXPECT_EQ(0, url_index.history_item_count_);
  EXPECT_TRUE(url_index.word_list_.empty());
  EXPECT_TRUE(url_index.word_map_.empty());
  EXPECT_TRUE(url_index.char_word_map_.empty());
  EXPECT_TRUE(url_index.word_id_history_map_.empty());
  EXPECT_TRUE(url_index.history_info_map_.empty());

  // Restore the cache.
  EXPECT_TRUE(url_index.RestorePrivateData(index_cache));

  // Compare the restored and captured for equality.
  EXPECT_EQ(history_item_count, url_index.history_item_count_);
  EXPECT_EQ(word_list.size(), url_index.word_list_.size());
  EXPECT_EQ(word_map.size(), url_index.word_map_.size());
  EXPECT_EQ(char_word_map.size(), url_index.char_word_map_.size());
  EXPECT_EQ(word_id_history_map.size(), url_index.word_id_history_map_.size());
  EXPECT_EQ(history_info_map.size(), url_index.history_info_map_.size());
  // WordList must be index-by-index equal.
  size_t count = word_list.size();
  for (size_t i = 0; i < count; ++i)
    EXPECT_EQ(word_list[i], url_index.word_list_[i]);
  for (InMemoryURLIndex::CharWordIDMap::const_iterator expected =
        char_word_map.begin(); expected != char_word_map.end(); ++expected) {
    InMemoryURLIndex::CharWordIDMap::const_iterator actual =
        url_index.char_word_map_.find(expected->first);
    ASSERT_TRUE(url_index.char_word_map_.end() != actual);
    const InMemoryURLIndex::WordIDSet& expected_set(expected->second);
    const InMemoryURLIndex::WordIDSet& actual_set(actual->second);
    ASSERT_EQ(expected_set.size(), actual_set.size());
    for (InMemoryURLIndex::WordIDSet::const_iterator set_iter =
        expected_set.begin(); set_iter != expected_set.end(); ++set_iter)
      EXPECT_GT(actual_set.count(*set_iter), 0U);
  }
  for (InMemoryURLIndex::WordIDHistoryMap::const_iterator expected =
      word_id_history_map.begin(); expected != word_id_history_map.end();
      ++expected) {
    InMemoryURLIndex::WordIDHistoryMap::const_iterator actual =
        url_index.word_id_history_map_.find(expected->first);
    ASSERT_TRUE(url_index.word_id_history_map_.end() != actual);
    const InMemoryURLIndex::HistoryIDSet& expected_set(expected->second);
    const InMemoryURLIndex::HistoryIDSet& actual_set(actual->second);
    ASSERT_EQ(expected_set.size(), actual_set.size());
    for (InMemoryURLIndex::HistoryIDSet::const_iterator set_iter =
        expected_set.begin(); set_iter != expected_set.end(); ++set_iter)
      EXPECT_GT(actual_set.count(*set_iter), 0U);
  }
  for (InMemoryURLIndex::HistoryInfoMap::const_iterator expected =
      history_info_map.begin(); expected != history_info_map.end();
      ++expected) {
    InMemoryURLIndex::HistoryInfoMap::const_iterator actual =
        url_index.history_info_map_.find(expected->first);
    ASSERT_FALSE(url_index.history_info_map_.end() == actual);
    const URLRow& expected_row(expected->second);
    const URLRow& actual_row(actual->second);
    EXPECT_EQ(expected_row.visit_count(), actual_row.visit_count());
    EXPECT_EQ(expected_row.typed_count(), actual_row.typed_count());
    EXPECT_EQ(expected_row.last_visit(), actual_row.last_visit());
    EXPECT_EQ(expected_row.url(), actual_row.url());
  }
}

}  // namespace history
