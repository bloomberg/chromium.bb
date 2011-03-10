// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>

#include <fstream>
#include <string>
#include <vector>

#include "app/sql/connection.h"
#include "app/sql/statement.h"
#include "app/sql/transaction.h"
#include "base/file_path.h"
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

  virtual FilePath::StringType TestDBName() const {
      return FILE_PATH_LITERAL("url_history_provider_test.db.txt");
  }

  URLRow MakeURLRow(const char* url,
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

  InMemoryURLIndex::String16Vector Make1Term(const char* term) {
    InMemoryURLIndex::String16Vector terms;
    terms.push_back(UTF8ToUTF16(term));
    return terms;
  }

  InMemoryURLIndex::String16Vector Make2Terms(const char* term_1,
                                              const char* term_2) {
    InMemoryURLIndex::String16Vector terms;
    terms.push_back(UTF8ToUTF16(term_1));
    terms.push_back(UTF8ToUTF16(term_2));
    return terms;
  }

  scoped_ptr<InMemoryURLIndex> url_index_;
};

class LimitedInMemoryURLIndexTest : public InMemoryURLIndexTest {
 protected:
  FilePath::StringType TestDBName() const {
    return FILE_PATH_LITERAL("url_history_provider_test_limited.db.txt");
  }
};

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
  EXPECT_EQ(36U, url_index_->char_word_map_.size());
  EXPECT_EQ(17U, url_index_->word_map_.size());
}

TEST_F(InMemoryURLIndexTest, Retrieval) {
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");
  InMemoryURLIndex::String16Vector terms;
  // The term will be lowercased by the search.

  // See if a very specific term gives a single result.
  terms.push_back(ASCIIToUTF16("DrudgeReport"));
  ScoredHistoryMatches matches = url_index_->HistoryItemsForTerms(terms);
  EXPECT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16("DRUDGE REPORT 2010"), matches[0].url_info.title());

  // Search which should result in multiple results.
  terms.clear();
  terms.push_back(ASCIIToUTF16("drudge"));
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(2U, url_index_->HistoryItemsForTerms(terms).size());
  // The results should be in descending score order.
  EXPECT_GT(matches[0].raw_score, matches[1].raw_score);

  // Search which should result in nearly perfect result.
  terms.clear();
  terms.push_back(ASCIIToUTF16("https"));
  terms.push_back(ASCIIToUTF16("NearlyPerfectResult"));
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());
  // The results should have a very high score.
  EXPECT_GT(matches[0].raw_score, 900);
  EXPECT_EQ(32, matches[0].url_info.id());
  EXPECT_EQ("https://nearlyperfectresult.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Perfect Search Result"),
            matches[0].url_info.title());

  // Search which should result in very poor result.
  terms.clear();
  terms.push_back(ASCIIToUTF16("z"));
  terms.push_back(ASCIIToUTF16("y"));
  terms.push_back(ASCIIToUTF16("x"));
  matches = url_index_->HistoryItemsForTerms(terms);
  ASSERT_EQ(1U, matches.size());
  // The results should have a poor score.
  EXPECT_LT(matches[0].raw_score, 200);
  EXPECT_EQ(33, matches[0].url_info.id());
  EXPECT_EQ("http://quiteuselesssearchresultxyz.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Useless Search Result"),
            matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, TitleSearch) {
  url_index_.reset(new InMemoryURLIndex());
  url_index_->Init(this, "en,ja,hi,zh");
  // Signal if someone has changed the test DB.
  EXPECT_EQ(28U, url_index_->history_info_map_.size());
  InMemoryURLIndex::String16Vector terms;

  // Ensure title is being searched.
  terms.push_back(ASCIIToUTF16("MORTGAGE"));
  terms.push_back(ASCIIToUTF16("RATE"));
  terms.push_back(ASCIIToUTF16("DROPS"));
  ScoredHistoryMatches matches = url_index_->HistoryItemsForTerms(terms);
  EXPECT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(1, matches[0].url_info.id());
  EXPECT_EQ("http://www.reuters.com/article/idUSN0839880620100708",
            matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16(
      "UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters"),
      matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, Char16Utilities) {
  string16 term = ASCIIToUTF16("drudgereport");
  string16 expected = ASCIIToUTF16("drugepot");
  EXPECT_EQ(expected.size(),
            InMemoryURLIndex::Char16SetFromString16(term).size());
  InMemoryURLIndex::Char16Vector c_vector =
      InMemoryURLIndex::Char16VectorFromString16(term);
  ASSERT_EQ(expected.size(), c_vector.size());

  InMemoryURLIndex::Char16Vector::const_iterator c_iter = c_vector.begin();
  for (string16::const_iterator s_iter = expected.begin();
       s_iter != expected.end(); ++s_iter, ++c_iter)
    EXPECT_EQ(*s_iter, *c_iter);
}

TEST_F(InMemoryURLIndexTest, StaticFunctions) {
  // Test WordVectorFromString16
  string16 string_a(ASCIIToUTF16("http://www.google.com/ frammy the brammy"));
  InMemoryURLIndex::String16Vector string_vec =
      InMemoryURLIndex::WordVectorFromString16(string_a, false);
  EXPECT_EQ(7U, string_vec.size());
  // See if we got the words we expected.
  EXPECT_EQ(UTF8ToUTF16("http"), string_vec[0]);
  EXPECT_EQ(UTF8ToUTF16("www"), string_vec[1]);
  EXPECT_EQ(UTF8ToUTF16("google"), string_vec[2]);
  EXPECT_EQ(UTF8ToUTF16("com"), string_vec[3]);
  EXPECT_EQ(UTF8ToUTF16("frammy"), string_vec[4]);
  EXPECT_EQ(UTF8ToUTF16("the"), string_vec[5]);
  EXPECT_EQ(UTF8ToUTF16("brammy"), string_vec[6]);

  string_vec = InMemoryURLIndex::WordVectorFromString16(string_a, true);
  EXPECT_EQ(5U, string_vec.size());
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

TEST_F(InMemoryURLIndexTest, TypedCharacterCaching) {
  // Verify that match results for previously typed characters are retained
  // (in the term_char_word_set_cache_) and reused, if possible, in future
  // autocompletes.
  url_index_.reset(new InMemoryURLIndex(FilePath(FILE_PATH_LITERAL("/dummy"))));
  url_index_->Init(this, "en,ja,hi,zh");

  // Verify that we can find something that already exists.
  InMemoryURLIndex::String16Vector terms;
  string16 term = ASCIIToUTF16("drudgerepo");
  terms.push_back(term);
  EXPECT_EQ(1U, url_index_->HistoryItemsForTerms(terms).size());

  {
    // Exercise the term matching cache with the same term.
    InMemoryURLIndex::Char16Vector uni_chars =
        InMemoryURLIndex::Char16VectorFromString16(term);
    EXPECT_EQ(uni_chars.size(), 7U);  // Equivalent to 'degopru'
    EXPECT_EQ(6U, url_index_->CachedResultsIndexForTerm(uni_chars));
  }

  {
    // Back off a character.
    InMemoryURLIndex::Char16Vector uni_chars =
        InMemoryURLIndex::Char16VectorFromString16(ASCIIToUTF16("drudgerep"));
    EXPECT_EQ(6U, uni_chars.size());  // Equivalent to 'degpru'
    EXPECT_EQ(5U, url_index_->CachedResultsIndexForTerm(uni_chars));
  }

  {
    // Add a couple of characters.
    InMemoryURLIndex::Char16Vector uni_chars =
        InMemoryURLIndex::Char16VectorFromString16(
            ASCIIToUTF16("drudgereporta"));
    EXPECT_EQ(9U, uni_chars.size());  // Equivalent to 'adegoprtu'
    EXPECT_EQ(6U, url_index_->CachedResultsIndexForTerm(uni_chars));
  }

  {
    // Use different string.
    InMemoryURLIndex::Char16Vector uni_chars =
        InMemoryURLIndex::Char16VectorFromString16(ASCIIToUTF16("abcde"));
    EXPECT_EQ(5U, uni_chars.size());
    EXPECT_EQ(static_cast<size_t>(-1),
              url_index_->CachedResultsIndexForTerm(uni_chars));
  }
}

TEST_F(InMemoryURLIndexTest, Scoring) {
  URLRow row_a(MakeURLRow("http://abcdef", "fedcba", 20, 0, 20));
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
  URLRow row_b(MakeURLRow("http://abcdef", "fedcba", 10, 0, 20));
  ScoredHistoryMatch scored_f(
      InMemoryURLIndex::ScoredMatchForURL(row_b, Make1Term("abc")));
  EXPECT_GT(scored_a.raw_score, scored_f.raw_score);
  // Test scores based on last_visit.
  URLRow row_c(MakeURLRow("http://abcdef", "fedcba", 20, 2, 20));
  ScoredHistoryMatch scored_g(
      InMemoryURLIndex::ScoredMatchForURL(row_c, Make1Term("abc")));
  EXPECT_GT(scored_a.raw_score, scored_g.raw_score);
  // Test scores based on typed_count.
  URLRow row_d(MakeURLRow("http://abcdef", "fedcba", 20, 0, 10));
  ScoredHistoryMatch scored_h(
      InMemoryURLIndex::ScoredMatchForURL(row_d, Make1Term("abc")));
  EXPECT_GT(scored_a.raw_score, scored_h.raw_score);
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
