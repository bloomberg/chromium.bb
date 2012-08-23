// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/browser/history/in_memory_url_index_base_unittest.h"
#include "chrome/browser/history/in_memory_url_index_types.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "sql/connection.h"
#include "third_party/sqlite/sqlite3.h"

using history::String16SetFromString16;

//------------------------------------------------------------------------------

// This class overrides the standard TestingProfile's disabling of the
// InMemoryURLIndexCacheDatabase.
class CacheTestingProfile : public TestingProfile {
 public:
  // Creates a testing profile but enables the cache database.
  bool InitHistoryService(HistoryService* history_service,
                          bool no_db) OVERRIDE;
};

bool CacheTestingProfile::InitHistoryService(HistoryService* history_service,
                                             bool no_db) {
  DCHECK(history_service);
  return history_service->Init(GetPath(),
                               reinterpret_cast<BookmarkService*>(
                                   BookmarkModelFactory::GetForProfile(this)),
                               no_db, false);
}

namespace history {

// -----------------------------------------------------------------------------

// Creates a URLRow with basic data for |url|, |title|, |visit_count|,
// |typed_count| and |id|. |last_visit_ago| gives the number of days from now
// to which to set the URL's last_visit.
URLRow MakeURLRowWithID(const char* url,
                        const char* title,
                        int visit_count,
                        int last_visit_ago,
                        int typed_count,
                        URLID id);

class InMemoryURLIndexTest : public InMemoryURLIndexBaseTest {
 protected:
  void SetUp() OVERRIDE;

  // Provides custom test data file name.
  virtual FilePath::StringType TestDBName() const OVERRIDE;

  // Validates that the given |term| is contained in |cache| and that it is
  // marked as in-use.
  void CheckTerm(const URLIndexPrivateData::SearchTermCacheMap& cache,
                 string16 term) const;
};

void InMemoryURLIndexTest::SetUp() {
  // Set things up without enabling the cache database.
  InMemoryURLIndexBaseTest::SetUp();
  LoadIndex();
}

FilePath::StringType InMemoryURLIndexTest::TestDBName() const {
    return FILE_PATH_LITERAL("url_history_provider_test.db.txt");
}

URLRow MakeURLRowWithID(const char* url,
                        const char* title,
                        int visit_count,
                        int last_visit_ago,
                        int typed_count,
                        URLID id) {
  URLRow row(GURL(url), id);
  row.set_title(UTF8ToUTF16(title));
  row.set_visit_count(visit_count);
  row.set_typed_count(typed_count);
  row.set_last_visit(base::Time::NowFromSystemTime() -
                     base::TimeDelta::FromDays(last_visit_ago));
  return row;
}

void InMemoryURLIndexTest::CheckTerm(
    const URLIndexPrivateData::SearchTermCacheMap& cache,
    string16 term) const {
  URLIndexPrivateData::SearchTermCacheMap::const_iterator cache_iter(
      cache.find(term));
  ASSERT_TRUE(cache.end() != cache_iter)
      << "Cache does not contain '" << term << "' but should.";
  URLIndexPrivateData::SearchTermCacheItem cache_item = cache_iter->second;
  EXPECT_TRUE(cache_item.used_)
      << "Cache item '" << term << "' should be marked as being in use.";
}

//------------------------------------------------------------------------------

class LimitedInMemoryURLIndexTest : public InMemoryURLIndexTest {
 protected:
  FilePath::StringType TestDBName() const OVERRIDE;
};

FilePath::StringType LimitedInMemoryURLIndexTest::TestDBName() const {
  return FILE_PATH_LITERAL("url_history_provider_test_limited.db.txt");
}

TEST_F(LimitedInMemoryURLIndexTest, Initialization) {
  // Verify that the database contains the expected number of items, which
  // is the pre-filtered count, i.e. all of the items.
  sql::Statement statement(
      history_database_->get_db_for_testing()->GetUniqueStatement(
          "SELECT * FROM urls;"));
  ASSERT_TRUE(statement.is_valid());
  uint64 row_count = 0;
  while (statement.Step()) ++row_count;
  EXPECT_EQ(1U, row_count);

  // history_info_map_ should have the same number of items as were filtered.
  EXPECT_EQ(1U, url_index_->private_data_->history_info_map_.size());
  EXPECT_EQ(35U, url_index_->private_data_->char_word_map_.size());
  EXPECT_EQ(17U, url_index_->private_data_->word_map_.size());
}

//------------------------------------------------------------------------------

TEST_F(InMemoryURLIndexTest, Retrieval) {
  // See if a very specific term gives a single result.
  // Note that in each case the term will be lowercased by the search.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("DrudgeReport"));
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16("DRUDGE REPORT 2010"), matches[0].url_info.title());
  EXPECT_TRUE(matches[0].can_inline);

  // Make sure a trailing space prevents inline-ability but still results
  // in the expected result.
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("DrudgeReport "));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(5, matches[0].url_info.id());
  EXPECT_EQ("http://drudgereport.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16("DRUDGE REPORT 2010"), matches[0].url_info.title());
  EXPECT_FALSE(matches[0].can_inline);

  // Search which should result in multiple results.
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("drudge"));
  ASSERT_EQ(2U, matches.size());
  // The results should be in descending score order.
  EXPECT_GE(matches[0].raw_score, matches[1].raw_score);

  // Search which should result in nearly perfect result.
  matches = url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("https NearlyPerfectResult"));
  ASSERT_EQ(1U, matches.size());
  // The results should have a very high score.
  EXPECT_GT(matches[0].raw_score, 900);
  EXPECT_EQ(32, matches[0].url_info.id());
  EXPECT_EQ("https://nearlyperfectresult.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Perfect Search Result"),
            matches[0].url_info.title());
  EXPECT_FALSE(matches[0].can_inline);

  // Search which should result in very poor result.
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("z y x"));
  ASSERT_EQ(1U, matches.size());
  // The results should have a poor score.
  EXPECT_LT(matches[0].raw_score, 500);
  EXPECT_EQ(33, matches[0].url_info.id());
  EXPECT_EQ("http://quiteuselesssearchresultxyz.com/",
            matches[0].url_info.url().spec());  // Note: URL gets lowercased.
  EXPECT_EQ(ASCIIToUTF16("Practically Useless Search Result"),
            matches[0].url_info.title());
  EXPECT_FALSE(matches[0].can_inline);

  // Search which will match at the end of an URL with encoded characters.
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("Mice"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(30, matches[0].url_info.id());
  EXPECT_FALSE(matches[0].can_inline);

  // Verify that a single term can appear multiple times in the URL and as long
  // as one starts the URL it is still inlined.
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("fubar"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(34, matches[0].url_info.id());
  EXPECT_EQ("http://fubarfubarandfubar.com/", matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16("Situation Normal -- FUBARED"),
            matches[0].url_info.title());
  EXPECT_TRUE(matches[0].can_inline);
}

TEST_F(InMemoryURLIndexTest, URLPrefixMatching) {
  // "drudgere" - found, can inline
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("drudgere"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "http://drudgere" - found, can inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("http://drudgere"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "www.atdmt" - not found
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("www.atdmt"));
  EXPECT_EQ(0U, matches.size());

  // "atdmt" - found, cannot inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("atdmt"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_FALSE(matches[0].can_inline);

  // "view.atdmt" - found, can inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("view.atdmt"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "http://view.atdmt" - found, can inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("http://view.atdmt"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "cnn.com" - found, can inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("cnn.com"));
  ASSERT_EQ(2U, matches.size());
  // One match should be inline-able, the other not.
  EXPECT_TRUE(matches[0].can_inline != matches[1].can_inline);

  // "www.cnn.com" - found, can inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("www.cnn.com"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "www.cnn.com" - found, cannot inline
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("ww.cnn.com"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(!matches[0].can_inline);

  // "http://www.cnn.com" - found, can inline
  matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("http://www.cnn.com"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].can_inline);

  // "tp://www.cnn.com" - found, cannot inline
  matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("tp://www.cnn.com"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(!matches[0].can_inline);
}

TEST_F(InMemoryURLIndexTest, ProperStringMatching) {
  // Search for the following with the expected results:
  // "atdmt view" - found
  // "atdmt.view" - not found
  // "view.atdmt" - found
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("atdmt view"));
  ASSERT_EQ(1U, matches.size());
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("atdmt.view"));
  ASSERT_EQ(0U, matches.size());
  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("view.atdmt"));
  ASSERT_EQ(1U, matches.size());
}

TEST_F(InMemoryURLIndexTest, HugeResultSet) {
  // Create a huge set of qualifying history items.
  for (URLID row_id = 5000; row_id < 6000; ++row_id) {
    URLRow new_row(GURL("http://www.brokeandaloneinmanitoba.com/"), row_id);
    new_row.set_last_visit(base::Time::Now());
    EXPECT_TRUE(UpdateURL(new_row));
  }

  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("b"));
  const URLIndexPrivateData* private_data(GetPrivateData());
  ASSERT_EQ(AutocompleteProvider::kMaxMatches, matches.size());
  // There are 7 matches already in the database.
  ASSERT_EQ(1008U, private_data->pre_filter_item_count_);
  ASSERT_EQ(500U, private_data->post_filter_item_count_);
  ASSERT_EQ(AutocompleteProvider::kMaxMatches,
            private_data->post_scoring_item_count_);
}

TEST_F(InMemoryURLIndexTest, TitleSearch) {
  // Signal if someone has changed the test DB.
  EXPECT_EQ(28U, GetPrivateData()->history_info_map_.size());

  // Ensure title is being searched.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("MORTGAGE RATE DROPS"));
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  EXPECT_EQ(1, matches[0].url_info.id());
  EXPECT_EQ("http://www.reuters.com/article/idUSN0839880620100708",
            matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16(
      "UPDATE 1-US 30-yr mortgage rate drops to new record low | Reuters"),
      matches[0].url_info.title());
}

TEST_F(InMemoryURLIndexTest, TitleChange) {
  // Verify current title terms retrieves desired item.
  string16 original_terms =
      ASCIIToUTF16("lebronomics could high taxes influence");
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(original_terms);
  ASSERT_EQ(1U, matches.size());

  // Verify that we got back the result we expected.
  const URLID expected_id = 3;
  EXPECT_EQ(expected_id, matches[0].url_info.id());
  EXPECT_EQ("http://www.businessandmedia.org/articles/2010/20100708120415.aspx",
            matches[0].url_info.url().spec());
  EXPECT_EQ(ASCIIToUTF16(
      "LeBronomics: Could High Taxes Influence James' Team Decision?"),
      matches[0].url_info.title());
  URLRow old_row(matches[0].url_info);

  // Verify new title terms retrieves nothing.
  string16 new_terms = ASCIIToUTF16("does eat oats little lambs ivy");
  matches = url_index_->HistoryItemsForTerms(new_terms);
  ASSERT_EQ(0U, matches.size());

  // Update the row.
  old_row.set_title(ASCIIToUTF16("Does eat oats and little lambs eat ivy"));
  EXPECT_TRUE(UpdateURL(old_row));

  // Verify we get the row using the new terms but not the original terms.
  matches = url_index_->HistoryItemsForTerms(new_terms);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(expected_id, matches[0].url_info.id());
  matches = url_index_->HistoryItemsForTerms(original_terms);
  ASSERT_EQ(0U, matches.size());
}

TEST_F(InMemoryURLIndexTest, NonUniqueTermCharacterSets) {
  // The presence of duplicate characters should succeed. Exercise by cycling
  // through a string with several duplicate characters.
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("ABRA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
  EXPECT_EQ("http://www.ddj.com/windows/184416623",
            matches[0].url_info.url().spec());

  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("ABRACAD"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("ABRACADABRA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("ABRACADABR"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());

  matches = url_index_->HistoryItemsForTerms(ASCIIToUTF16("ABRACA"));
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(28, matches[0].url_info.id());
}

TEST_F(InMemoryURLIndexTest, TypedCharacterCaching) {
  // Verify that match results for previously typed characters are retained
  // (in the term_char_word_set_cache_) and reused, if possible, in future
  // autocompletes.
  typedef URLIndexPrivateData::SearchTermCacheMap::iterator CacheIter;
  typedef URLIndexPrivateData::SearchTermCacheItem CacheItem;

  const URLIndexPrivateData::SearchTermCacheMap& cache(
      GetPrivateData()->search_term_cache_);

  // The cache should be empty at this point.
  EXPECT_EQ(0U, cache.size());

  // Now simulate typing search terms into the omnibox and check the state of
  // the cache as each item is 'typed'.

  // Simulate typing "r" giving "r" in the simulated omnibox. The results for
  // 'r' will be not cached because it is only 1 character long.
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("r"));
  EXPECT_EQ(0U, cache.size());

  // Simulate typing "re" giving "r re" in the simulated omnibox.
  // 're' should be cached at this point but not 'r' as it is a single
  // character.
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("r re"));
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, ASCIIToUTF16("re"));

  // Simulate typing "reco" giving "r re reco" in the simulated omnibox.
  // 're' and 'reco' should be cached at this point but not 'r' as it is a
  // single character.
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("r re reco"));
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, ASCIIToUTF16("re"));
  CheckTerm(cache, ASCIIToUTF16("reco"));

  // Simulate typing "mort".
  // Since we now have only one search term, the cached results for 're' and
  // 'reco' should be purged, giving us only 1 item in the cache (for 'mort').
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("mort"));
  ASSERT_EQ(1U, cache.size());
  CheckTerm(cache, ASCIIToUTF16("mort"));

  // Simulate typing "reco" giving "mort reco" in the simulated omnibox.
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("mort reco"));
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, ASCIIToUTF16("mort"));
  CheckTerm(cache, ASCIIToUTF16("reco"));

  // Simulate a <DELETE> by removing the 'reco' and adding back the 'rec'.
  url_index_->HistoryItemsForTerms(ASCIIToUTF16("mort rec"));
  ASSERT_EQ(2U, cache.size());
  CheckTerm(cache, ASCIIToUTF16("mort"));
  CheckTerm(cache, ASCIIToUTF16("rec"));
}

TEST_F(InMemoryURLIndexTest, AddNewRows) {
  // Verify that the row we're going to add does not already exist.
  URLID new_row_id = 87654321;
  // Newly created URLRows get a last_visit time of 'right now' so it should
  // qualify as a quick result candidate.
  EXPECT_TRUE(url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("brokeandalone")).empty());

  // Add a new row.
  URLRow new_row(GURL("http://www.brokeandaloneinmanitoba.com/"), new_row_id++);
  new_row.set_last_visit(base::Time::Now());
  EXPECT_TRUE(UpdateURL(new_row));

  // Verify that we can retrieve it.
  EXPECT_EQ(1U, url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("brokeandalone")).size());

  // Add it again just to be sure that is harmless and that it does not update
  // the index.
  EXPECT_FALSE(UpdateURL(new_row));
  EXPECT_EQ(1U, url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("brokeandalone")).size());

  // Make up an URL that does not qualify and try to add it.
  URLRow unqualified_row(GURL("http://www.brokeandaloneinmanitoba.com/"),
                         new_row_id++);
  EXPECT_FALSE(UpdateURL(new_row));
}

TEST_F(InMemoryURLIndexTest, DeleteRows) {
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("DrudgeReport"));
  ASSERT_EQ(1U, matches.size());

  // Delete the URL then search again.
  EXPECT_TRUE(DeleteURL(matches[0].url_info.url()));
  EXPECT_TRUE(url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("DrudgeReport")).empty());

  // Make up an URL that does not exist in the database and delete it.
  GURL url("http://www.hokeypokey.com/putyourrightfootin.html");
  EXPECT_FALSE(DeleteURL(url));
}

TEST_F(InMemoryURLIndexTest, ExpireRow) {
  ScoredHistoryMatches matches =
      url_index_->HistoryItemsForTerms(ASCIIToUTF16("DrudgeReport"));
  ASSERT_EQ(1U, matches.size());

  // Determine the row id for the result, remember that id, broadcast a
  // delete notification, then ensure that the row has been deleted.
  URLsDeletedDetails deleted_details;
  deleted_details.all_history = false;
  deleted_details.rows.push_back(matches[0].url_info);
  content::Source<InMemoryURLIndexTest> source(this);
  url_index_->Observe(
      chrome::NOTIFICATION_HISTORY_URLS_DELETED,
      content::Source<InMemoryURLIndexTest>(this),
      content::Details<history::HistoryDetails>(&deleted_details));
  EXPECT_TRUE(url_index_->HistoryItemsForTerms(
      ASCIIToUTF16("DrudgeReport")).empty());
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

  std::set<std::string> whitelist;
  URLIndexPrivateData::InitializeSchemeWhitelistForTesting(&whitelist);
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    GURL url(data[i].url_spec);
    EXPECT_EQ(data[i].expected_is_whitelisted,
              URLIndexPrivateData::URLSchemeIsWhitelisted(url, whitelist));
  }
}

// InMemoryURLIndexCacheTest ---------------------------------------------------

class InMemoryURLIndexCacheTest : public InMemoryURLIndexBaseTest {
 public:
  // A test helper class which captures the state of the URL index and cache
  // prior to the addition, update or delete of a history URL visit, determines
  // the expected state after the URL change, and verifies the actual state.
  class CacheChecker {
   public:
      enum ChangeType {
        URL_ADDED,
        URL_UPDATED,
        URL_REMOVED,
      };

    // Captures the state of the index and cache and the changes which are
    // expected to be made. |index| points to the test index. |change_type|
    // specifies the kind of history URL visit change that will take place.
    // |existing_words| lists the words that are referenced by the history item
    // (both URL and page title) which are expected to already be recorded in
    // the index. |added_words| gives terms which are not currently in the index
    // but will be added for this history item. |removed_words| gives words in
    // the index but which are not referenced by any other history item and
    // which will be removed by the test action.
    CacheChecker(InMemoryURLIndex* index,
                 ChangeType change_type,
                 URLID history_id,
                 const String16Set& existing_words,
                 const String16Set& added_words,
                 const String16Set& removed_words);
    virtual ~CacheChecker();

    // Initializes and destroys things in a way that the standard gtest
    // EXPECT_xxx macros can be used since those macros cannot be used in
    // constructors and destructors.
    void Init();
    void Destroy();

   private:
    // A helper struct that records index current state for words in the list of
    // |existing_words| and |removed_words|.
    struct ExistingWord {
      ExistingWord() : word_id(0), word_map_count(0), word_table_count(0) {}
      ExistingWord(WordID word_id,
                   size_t word_map_count,
                   size_t word_table_count)
          : word_id(word_id),
            word_map_count(word_map_count),
            word_table_count(word_table_count) {}

      WordID word_id;  // Used for removed_words only.
      size_t word_map_count;
      size_t word_table_count;
    };

    // Table names.
    static const char* kWordsTableName;
    static const char* kWordHistoryTableName;
    static const char* kURLsTableName;
    static const char* kURLWordStartsTableName;
    static const char* kTitleWordStartsTableName;
    // Field names.
    static const char* kWord;
    static const char* kWordID;
    static const char* kHistoryID;
    static const char* kWordStart;

    // Helper functions that perform a COUNT query of the cache database with
    // the given criteria and return the number of rows meeting that criteria.
    // |table| specifies the name of the table in which the SELECT will be
    // performed. |column| gives the column against which the desired |word|,
    // |word_id| or |history_id| will be matched.
    int SelectCount(const char* table);
    int SelectCount(const char* table,
                    const char* column,
                    const string16& word);
    int SelectCount(const char* table,
                    const char* column,
                    WordID word_id);
    int SelectCount(const char* table,
                    const char* column,
                    HistoryID history_id);

    InMemoryURLIndex& index_;
    sql::Connection* db_;
    // Information about the changes that the test is expected to cause.
    ChangeType change_type_;
    URLID history_id_;
    String16Set existing_words_;
    String16Set added_words_;
    String16Set removed_words_;
    // Current state information about the index.
    size_t word_list_count_;
    size_t word_table_count;
    size_t history_id_word_map_count_;
    size_t available_words_count_;
    std::map<string16, ExistingWord> existing_words_and_counts_;
    std::map<string16, ExistingWord> removed_words_and_counts_;
  };

  void SetUp() OVERRIDE;

  // Provides custom test data file name.
  virtual FilePath::StringType TestDBName() const OVERRIDE;
};

// InMemoryURLIndexCacheTest::CacheChecker -------------------------------------

// Cache database table names. See in_memory_url_cache_database.cc.
typedef InMemoryURLIndexCacheTest::CacheChecker CacheChecker;
const char* CacheChecker::kWordsTableName = "words";
const char* CacheChecker::kWordHistoryTableName = "word_history";
const char* CacheChecker::kURLsTableName = "urls";
const char* CacheChecker::kURLWordStartsTableName = "url_word_starts";
const char* CacheChecker::kTitleWordStartsTableName = "title_word_starts";

// Cache database table field names.
const char* CacheChecker::kWord = "word";
const char* CacheChecker::kWordID = "word_id";
const char* CacheChecker::kHistoryID = "history_id";
const char* CacheChecker::kWordStart = "word_start";

CacheChecker::CacheChecker(
    InMemoryURLIndex* index,
    ChangeType change_type,
    URLID history_id,
    const String16Set& existing_words,
    const String16Set& added_words,
    const String16Set& removed_words)
    : index_(*index),
      db_(index_.private_data_->cache_db()->get_db_for_testing()),
      change_type_(change_type),
      history_id_(history_id),
      existing_words_(existing_words),
      added_words_(added_words),
      removed_words_(removed_words),
      // Remember the old word count, the old word table count, the old history
      // ID count, and how many unused words are available for reuse.
      word_list_count_(index_.private_data_->word_list_.size()),
      word_table_count(SelectCount(kWordsTableName)),
      history_id_word_map_count_(
          index_.private_data_->history_id_word_map_.size()),
      available_words_count_(index_.private_data_->available_words_.size()) {
  DCHECK(db_);
  Init();
}

void CacheChecker::Init() {
  // Verify that the existing words exist and remember their counts.
  URLIndexPrivateData* private_data(index_.private_data_.get());
  for (String16Set::iterator word_iter = existing_words_.begin();
       word_iter != existing_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    ASSERT_NE(0U, private_data->word_map_.count(word))
        << "Existing word '" << word << "' not found.";
    EXPECT_GT(SelectCount(kWordsTableName, kWord, word), 0)
        << "Existing word '" << word << "' not found in words table.";
    WordID word_id = private_data->word_map_[word];
    EXPECT_GT(SelectCount(kWordHistoryTableName, kWordID, word_id), 0)
        << "Existing word '" << word << "' not found in word_history table.";
    existing_words_and_counts_[word] =
        ExistingWord(word_id, private_data->word_id_history_map_[word_id].
            size(),
            SelectCount(kWordHistoryTableName, kWordID, word_id));
  }

  // Verify that the added words don't already exist.
  for (String16Set::iterator word_iter = added_words_.begin();
       word_iter != added_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    EXPECT_EQ(0U, private_data->word_map_.count(word))
        << "Word '" << word << "' to be added is already there.";
    EXPECT_EQ(0, SelectCount(kWordsTableName, kWord, word))
        << "Word '" << word << "' to be added is already in words table.";
  }

  // Verify that the removed words exist and remember their counts.
  for (String16Set::iterator word_iter = removed_words_.begin();
       word_iter != removed_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    ASSERT_EQ(1U, private_data->word_map_.count(word))
        << "Word '" << word << "' to be removed not found.";
    WordID word_id = private_data->word_map_[word];
    EXPECT_GT(private_data->word_id_history_map_[word_id].size(), 0U);
    EXPECT_GT(SelectCount(kWordsTableName, kWord, word), 0)
        << "Word '" << word << "' to be removed not found in words table.";
    EXPECT_GT(SelectCount(kWordHistoryTableName, kWordID, word_id), 0)
        << "Word '" << word << "' to remove not found in word_history table.";
    removed_words_and_counts_[word] =
        ExistingWord(word_id, private_data->word_id_history_map_[word_id].
            size(),
            SelectCount(kWordHistoryTableName, kWordID, word_id));
    EXPECT_EQ(removed_words_and_counts_[word].word_map_count,
              removed_words_and_counts_[word].word_table_count);
  }
}

CacheChecker::~CacheChecker() {
  Destroy();
}

void CacheChecker::Destroy() {
  // Verify that the existing words still exist and their counts have
  // incremented by 1.
  URLIndexPrivateData* private_data(index_.private_data_.get());
  for (String16Set::iterator word_iter = existing_words_.begin();
       word_iter != existing_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    const ExistingWord& existing_word(existing_words_and_counts_[word]);
    size_t expected_count = existing_word.word_map_count;
    if (change_type_ == URL_ADDED)
      ++expected_count;
    WordID word_id = private_data->word_map_[word];
    EXPECT_EQ(expected_count,
              private_data->word_id_history_map_[word_id].size())
        << "Occurrence count for existing word '" << word
        << "' in the word_id_history_map_ is incorrect.";
    EXPECT_EQ(static_cast<int>(expected_count),
              SelectCount(kWordHistoryTableName, kWordID, word_id))
        << "Occurrence count for existing word '" << word << "' in "
           "the word_history database table is incorrect.";
  }

  // Verify the added words have been added and their counts are 1.
  for (String16Set::iterator word_iter = added_words_.begin();
       word_iter != added_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    ASSERT_EQ(1U, private_data->word_map_.count(word));
    WordID word_id = private_data->word_map_[word];
    EXPECT_EQ(1U, private_data->word_id_history_map_[word_id].size())
        << "Count for added word '" << word << "' not 1.";
    EXPECT_EQ(1, SelectCount(kWordsTableName, kWord, word))
        << "Word '" << word << "' not added to words table.";
    EXPECT_EQ(1, SelectCount(kWordHistoryTableName, kWordID, word_id))
        << "Word '" << word << "' not added to word_history table.";
  }

  switch (change_type_) {
    case URL_ADDED:
      ASSERT_EQ(0U, removed_words_.size())
          << "BAD TEST DATA -- removed_words must be empty for URL_ADDED.";
      // Fall through.
    case URL_UPDATED: {
      // There should be added-words + existing-words - removed-words entries
      // in the word_history table for the history ID.
      size_t word_count_for_id = existing_words_.size() + added_words_.size();
      EXPECT_EQ(word_count_for_id,
                private_data->history_id_word_map_[history_id_].size());
      EXPECT_EQ(static_cast<int>(word_count_for_id),
                SelectCount(kWordHistoryTableName, kHistoryID, history_id_));
      EXPECT_EQ(1U, private_data->history_id_word_map_.count(history_id_));
      EXPECT_EQ(1, SelectCount(kURLsTableName, kHistoryID, history_id_));
    }
    break;
    case URL_REMOVED:
      // There should be no entries in the word_history table for the history
      // ID.
      ASSERT_EQ(0U, added_words_.size())
          << "BAD TEST DATA -- added_words must be empty for URL_REMOVED.";
      EXPECT_EQ(0U, private_data->history_id_word_map_.count(history_id_));
      EXPECT_EQ(0, SelectCount(kWordHistoryTableName, kHistoryID, history_id_));
      EXPECT_EQ(0, SelectCount(kURLsTableName, kHistoryID, history_id_));
      break;
  }

  // Verify that the count for removed words has been decremented or that
  // they have been deleted if their count has dropped to 0.
  int completely_removed = 0;
  for (String16Set::iterator word_iter = removed_words_.begin();
       word_iter != removed_words_.end(); ++word_iter) {
    string16 word(*word_iter);
    const ExistingWord& removed_word(removed_words_and_counts_[word]);
    size_t expected_word_count = removed_word.word_map_count - 1;
    if (expected_word_count > 0) {
      EXPECT_EQ(1, SelectCount(kWordsTableName, kWord, word))
          << "Word '" << word << "' not removed from words table.";
      ASSERT_EQ(1U, private_data->word_map_.count(word))
          << "Word '" << word << "' is gone but should still be there.";
      EXPECT_EQ(expected_word_count,
                private_data->word_id_history_map_[removed_word.word_id].size())
          << "Count for existing word '" << word << "' not decremented. A";
      EXPECT_EQ(static_cast<int>(expected_word_count),
                SelectCount(kWordHistoryTableName, kWordID,
                    removed_word.word_id))
          << "Count for existing word '" << word << "' not decremented. B";
    } else {
      EXPECT_EQ(0, SelectCount(kWordsTableName, kWord, word))
          << "Word '" << word << "' not removed from words table.";
      EXPECT_EQ(0U, private_data->word_map_.count(word))
          << "Word '" << word << "' to be removed is still there.";
      ++completely_removed;
    }
  }

  // Verify that the size of the in-memory and on-disk database tables have
  // changed as expected.
  EXPECT_EQ(std::max(0, static_cast<int>(available_words_count_) +
      completely_removed - static_cast<int>(added_words_.size())),
      static_cast<int>(private_data->available_words_.size()));
  // The in-memory table will never get smaller as we remember the freed-up
  // slots and reuse them.
  EXPECT_EQ(static_cast<int>(word_list_count_) + std::max(0,
      static_cast<int>(added_words_.size()) - completely_removed),
      static_cast<int>(private_data->word_list_.size()));
  EXPECT_EQ(static_cast<int>(word_table_count) +
      static_cast<int>(added_words_.size()) - completely_removed,
      SelectCount(kWordsTableName));
}

int CacheChecker::SelectCount(const char* table) {
  std::string sql(StringPrintf("SELECT COUNT(*) FROM %s", table));
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  return (statement.Step()) ? statement.ColumnInt(0) : -1;
}

int CacheChecker::SelectCount(const char* table,
                              const char* column,
                              const string16& word) {
  std::string sql(StringPrintf("SELECT COUNT(*) FROM %s WHERE %s = ?",
                               table, column));
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  statement.BindString16(0, word);
  return (statement.Step()) ? statement.ColumnInt(0) : -1;
}

int CacheChecker::SelectCount(
    const char* table,
    const char* column,
    WordID word_id) {
  std::string sql(StringPrintf("SELECT COUNT(*) FROM %s WHERE %s = ?",
                               table, column));
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  statement.BindInt(0, word_id);
  return (statement.Step()) ? statement.ColumnInt(0) : -1;
}

int CacheChecker::SelectCount(const char* table,
                              const char* column,
                              HistoryID history_id) {
  std::string sql(StringPrintf("SELECT COUNT(*) FROM %s WHERE %s = ?",
                               table, column));
  sql::Statement statement(db_->GetUniqueStatement(sql.c_str()));
  statement.BindInt(0, history_id);
  return (statement.Step()) ? statement.ColumnInt(0) : -1;
}

// InMemoryURLIndexCacheTest ---------------------------------------------------

void InMemoryURLIndexCacheTest::SetUp() {
  profile_.reset(new CacheTestingProfile);
  InMemoryURLIndexBaseTest::SetUp();
  LoadIndex();
  DCHECK(url_index_->index_available());
}

FilePath::StringType InMemoryURLIndexCacheTest::TestDBName() const {
  return FILE_PATH_LITERAL("url_history_provider_test.db.txt");
}

TEST_F(InMemoryURLIndexCacheTest, CacheAddRemove) {
  // Initialize the cache then add and remove some history items.
  const URLID kNewRowID = 250;
  URLRow new_row(MakeURLRowWithID("http://www.frank-and-earnest.com/",
                                  "Frank and Earnest Go Crash in Washington",
                                  10, 0, 5, kNewRowID));

  {
    // Add a new URL.
    String16Set existing_words(String16SetFromString16(
        UTF8ToUTF16("http www and com crash in"), NULL));
    // New words: frank, earnest, go, washington.
    String16Set added_words(String16SetFromString16(
        UTF8ToUTF16("frank earnest go washington"), NULL));
    String16Set removed_words;
    CacheChecker cache_checker(url_index_, CacheChecker::URL_ADDED,
        kNewRowID, existing_words, added_words, removed_words);
    UpdateURL(new_row);
  }

  URLRow old_row(new_row);
  {
    // Update an existing URL resulting in a net addition of words.
    old_row.set_title(UTF8ToUTF16(
        "Frank and Earnest Go Crazy in Draper Utah USA"));
    String16Set existing_words(String16SetFromString16(
        UTF8ToUTF16("http www and com in frank earnest go"), NULL));
    String16Set added_words(String16SetFromString16(
        UTF8ToUTF16("crazy draper utah usa"), NULL));
    String16Set removed_words(String16SetFromString16(
        UTF8ToUTF16("washington crash"), NULL));
    CacheChecker cache_checker(url_index_, CacheChecker::URL_UPDATED,
        kNewRowID, existing_words, added_words, removed_words);
    UpdateURL(old_row);
  }

  {
    // Update an existing URL resulting in a net removal of words.
    old_row.set_title(UTF8ToUTF16("Frank and Earnest Go Crazy Crazy"));
    String16Set existing_words(String16SetFromString16(
        UTF8ToUTF16("http www and com frank earnest go crazy"), NULL));
    String16Set added_words;
    String16Set removed_words(String16SetFromString16(
        UTF8ToUTF16("in draper utah usa"), NULL));
    CacheChecker cache_checker(url_index_, CacheChecker::URL_UPDATED,
        kNewRowID, existing_words, added_words, removed_words);
    UpdateURL(old_row);
  }

  {
    // Delete an existing URL.
    old_row.set_title(UTF8ToUTF16("Frank and Earnest Go Crazy Crazy"));
    String16Set existing_words;
    String16Set added_words;
    String16Set removed_words(String16SetFromString16(
        UTF8ToUTF16("http www and com frank earnest go crazy"), NULL));
    CacheChecker cache_checker(url_index_, CacheChecker::URL_REMOVED,
        kNewRowID, existing_words, added_words, removed_words);
    DeleteURL(old_row.url());
  }
}

// InterposingCacheDatabase ----------------------------------------------------

// This class allows certain InMemoryURLCacheDatabase methods to be intercepted
// for purposes of recording or simulating failures.
class InterposingCacheDatabase : public InMemoryURLCacheDatabase {
 public:
  InterposingCacheDatabase();

  virtual bool Reset() OVERRIDE;
  virtual bool Refresh(const URLIndexPrivateData& index_data) OVERRIDE;
  virtual void AddWordToWordsTask(WordID word_id,
                                  const string16& uni_word) OVERRIDE;

  void set_simulate_update_fail(bool fail) { simulate_update_fail_ = fail; }
  void set_simulate_refresh_fail(bool fail) { simulate_refresh_fail_ = fail; }
  void set_tracking_calls(bool tracking) { tracking_calls_ = tracking; }
  int reset_calling_sequence() const { return reset_calling_sequence_; }
  int refresh_calling_sequence() const { return refresh_calling_sequence_; }
  void set_update_error(int error) { update_error_ = error; }
  bool failure_occurred() const { return update_error_ != SQLITE_OK; }

 private:
  virtual ~InterposingCacheDatabase() {}

  bool simulate_update_fail_;
  bool simulate_refresh_fail_;
  int next_calling_sequence_;
  bool tracking_calls_;
  int reset_calling_sequence_;
  int refresh_calling_sequence_;
};

InterposingCacheDatabase::InterposingCacheDatabase()
    : simulate_update_fail_(false),
      simulate_refresh_fail_(false),
      next_calling_sequence_(0),
      tracking_calls_(false),
      reset_calling_sequence_(-1),
      refresh_calling_sequence_(-1) {
}

bool InterposingCacheDatabase::Reset() {
  bool success = InMemoryURLCacheDatabase::Reset();
  if (tracking_calls_)
    reset_calling_sequence_ = next_calling_sequence_++;
  return success;
}

bool InterposingCacheDatabase::Refresh(
    const URLIndexPrivateData& index_data) {
  bool success =
      !simulate_refresh_fail_ && InMemoryURLCacheDatabase::Refresh(index_data);
  if (tracking_calls_)
    refresh_calling_sequence_ = next_calling_sequence_++;
  return success;
}

void InterposingCacheDatabase::AddWordToWordsTask(
    WordID word_id,
    const string16& uni_word) {
  if (simulate_update_fail_)
    set_update_error(SQLITE_CORRUPT);
  else
    InMemoryURLCacheDatabase::AddWordToWordsTask(word_id, uni_word);
}

// IntercessionaryIndexTest ----------------------------------------------------

// Tests which intercede in some way with the normal operation of the index,
// its private data, and/or the cache database.
class IntercessionaryIndexTest : public InMemoryURLIndexBaseTest {
 public:
  IntercessionaryIndexTest();
  ~IntercessionaryIndexTest() {}

  void SetUp() OVERRIDE;

  // Provides custom test data file name.
  virtual FilePath::StringType TestDBName() const OVERRIDE;

 protected:
  scoped_refptr<InterposingCacheDatabase> test_db_;
  URLIndexPrivateData* private_data_;
};

IntercessionaryIndexTest::IntercessionaryIndexTest()
    : test_db_(new InterposingCacheDatabase) {
}

void IntercessionaryIndexTest::SetUp() {
  // Set things up without enabling the cache database.
  InMemoryURLIndexBaseTest::SetUp();
  LoadIndex();
  DCHECK(url_index_->index_available());

  // Enabled our custom database and refresh it.
  FilePath dir_path;
  private_data_ = GetPrivateData();
  DCHECK(private_data_->GetCacheDBPath(&dir_path));
  base::SequencedWorkerPool::SequenceToken sequence_token =
      url_index_->sequence_token_for_testing();
  content::BrowserThread::GetBlockingPool()->PostWorkerTask(FROM_HERE,
      base::Bind(base::IgnoreResult(&InMemoryURLCacheDatabase::Init),
                 test_db_, dir_path, sequence_token));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  private_data_->SetCacheDatabaseForTesting(test_db_);
  private_data_->set_cache_enabled(true);
  content::BrowserThread::GetBlockingPool()->PostWorkerTask(FROM_HERE,
      base::Bind(&URLIndexPrivateData::RefreshCacheTask, private_data_));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  test_db_->set_tracking_calls(true);
}

FilePath::StringType IntercessionaryIndexTest::TestDBName() const {
  return FILE_PATH_LITERAL("url_history_provider_test.db.txt");
}

TEST_F(IntercessionaryIndexTest, CacheDatabaseFailure) {
  // Perform an update but do not fail.
  const URLID kNewRowID = 250;
  URLRow new_row(MakeURLRowWithID("http://www.frank-and-earnest.com/",
                                  "Frank and Earnest Go Crash in Washington",
                                  10, 0, 5, kNewRowID));
  UpdateURL(new_row);
  EXPECT_EQ(-1, test_db_->reset_calling_sequence());
  EXPECT_EQ(-1, test_db_->refresh_calling_sequence());
  EXPECT_FALSE(test_db_->failure_occurred());
  EXPECT_TRUE(private_data_->cache_enabled());

  // Perform an update that fails but remediation succeeds.
  test_db_->set_simulate_update_fail(true);
  URLRow old_row(new_row);
  old_row.set_title(UTF8ToUTF16(
      "Frank and Earnest Go Crazy in Draper Utah USA"));
  content::WindowedNotificationObserver update_failure_observer(
      chrome::NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE,
      content::NotificationService::AllSources());
  UpdateURL(old_row);
  update_failure_observer.Wait();
  // Wait for the pending reset and refresh to complete.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  EXPECT_EQ(0, test_db_->reset_calling_sequence());
  EXPECT_EQ(1, test_db_->refresh_calling_sequence());
  EXPECT_TRUE(test_db_->failure_occurred());
  EXPECT_TRUE(private_data_->cache_enabled());

  // Perform an update that fails and remediation fails.
  test_db_->set_simulate_update_fail(true);
  test_db_->set_simulate_refresh_fail(true);
  test_db_->set_update_error(SQLITE_OK);
  old_row.set_title(UTF8ToUTF16(
      "Frank and Earnest Light Up Dizzy World"));
  content::WindowedNotificationObserver refresh_failure_observer(
      chrome::NOTIFICATION_IN_MEMORY_URL_CACHE_DATABASE_FAILURE,
      content::NotificationService::AllSources());
  UpdateURL(old_row);
  refresh_failure_observer.Wait();
  // Wait for the pending reset and refresh to complete.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  EXPECT_EQ(2, test_db_->reset_calling_sequence());
  EXPECT_EQ(3, test_db_->refresh_calling_sequence());
  EXPECT_TRUE(test_db_->failure_occurred());
  EXPECT_FALSE(private_data_->cache_enabled());
}

TEST_F(IntercessionaryIndexTest, ShutdownDuringCacheRefresh) {
  // Pretend that the cache is dysfunctional.
  private_data_->set_cache_enabled(false);
  // Kick off a refresh and immediately shut down.
  url_index_->PostRefreshCacheTask();
  url_index_->Shutdown();
  // The index should still be viable but the cache should have been shut down.
  EXPECT_FALSE(private_data_->cache_enabled());
  EXPECT_TRUE(url_index_->index_available());
}

}  // namespace history
