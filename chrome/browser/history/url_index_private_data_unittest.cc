// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/path_service.h"
#include "base/scoped_temp_dir.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/in_memory_url_cache_database.h"
#include "chrome/browser/history/url_index_private_data.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

struct TestURLInfo {
  std::string url;
  std::string title;
  int visit_count;
  int typed_count;
  int days_from_now;
} private_data_test_db[] = {
  {"http://www.google.com/", "Google", 3, 3, 0},
  {"http://slashdot.org/favorite_page.html", "Favorite page", 200, 100, 0},
  {"http://kerneltrap.org/not_very_popular.html", "Less popular", 4, 0, 0},
  {"http://freshmeat.net/unpopular.html", "Unpopular", 1, 1, 0},
  {"http://news.google.com/?ned=us&topic=n", "Google News - U.S.", 2, 2, 0},
  {"http://news.google.com/", "Google News", 1, 1, 0},
  {"http://foo.com/", "Dir", 200, 100, 0},
  {"http://foo.com/dir/", "Dir", 2, 1, 10},
  {"http://foo.com/dir/another/", "Dir", 5, 10, 0},
  {"http://foo.com/dir/another/again/", "Dir", 5, 1, 0},
  {"http://foo.com/dir/another/again/myfile.html", "File", 3, 2, 0},
  {"http://visitedest.com/y/a", "VA", 10, 1, 20},
  {"http://visitedest.com/y/b", "VB", 9, 1, 20},
  {"http://visitedest.com/x/c", "VC", 8, 1, 20},
  {"http://visitedest.com/x/d", "VD", 7, 1, 20},
  {"http://visitedest.com/y/e", "VE", 6, 1, 20},
  {"http://typeredest.com/y/a", "TA", 3, 5, 0},
  {"http://typeredest.com/y/b", "TB", 3, 4, 0},
  {"http://typeredest.com/x/c", "TC", 3, 3, 0},
  {"http://typeredest.com/x/d", "TD", 3, 2, 0},
  {"http://typeredest.com/y/e", "TE", 3, 1, 0},
  {"http://daysagoest.com/y/a", "DA", 1, 1, 0},
  {"http://daysagoest.com/y/b", "DB", 1, 1, 1},
  {"http://daysagoest.com/x/c", "DC", 1, 1, 2},
  {"http://daysagoest.com/x/d", "DD", 1, 1, 3},
  {"http://daysagoest.com/y/e", "DE", 1, 1, 4},
  {"http://abcdefghixyzjklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://spaces.com/path%20with%20spaces/foo.html", "Spaces", 2, 2, 0},
  {"http://abcdefghijklxyzmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcdefxyzghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://abcxyzdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://xyzabcdefghijklmnopqrstuvw.com/a", "", 3, 1, 0},
  {"http://cda.com/Dogs%20Cats%20Gorillas%20Sea%20Slugs%20and%20Mice",
   "Dogs & Cats & Mice & Other Animals", 1, 1, 0},
  {"https://monkeytrap.org/", "", 3, 1, 0},
};

class URLIndexPrivateDataTest : public testing::Test {
 public:
  URLIndexPrivateDataTest();
  virtual ~URLIndexPrivateDataTest();

  virtual void SetUp() OVERRIDE;

  void GetTestData(size_t* data_count, TestURLInfo** test_data);

  // Fills test data into the history system.
  void FillData();

  // Data verification helper functions.
  void ExpectPrivateDataNotEmpty(const URLIndexPrivateData& data);
  void ExpectPrivateDataEqual(const URLIndexPrivateData& expected,
                              const URLIndexPrivateData& actual);

 protected:
  scoped_refptr<URLIndexPrivateData> private_data_;
  ScopedTempDir temp_cache_dir_;
};

URLIndexPrivateDataTest::URLIndexPrivateDataTest() {}
URLIndexPrivateDataTest::~URLIndexPrivateDataTest() {}

void URLIndexPrivateDataTest::SetUp() {
  ASSERT_TRUE(temp_cache_dir_.CreateUniqueTempDir());
  private_data_ = new URLIndexPrivateData(temp_cache_dir_.path(), "en");
  private_data_->Init(
      content::BrowserThread::GetBlockingPool()->GetSequenceToken());
  FillData();
}

void URLIndexPrivateDataTest::GetTestData(size_t* data_count,
                                          TestURLInfo** test_data) {
  DCHECK(data_count);
  DCHECK(test_data);
  *data_count = arraysize(private_data_test_db);
  *test_data = &private_data_test_db[0];
}

void URLIndexPrivateDataTest::FillData() {
  size_t data_count = 0;
  TestURLInfo* test_data = NULL;
  GetTestData(&data_count, &test_data);
  for (size_t i = 0; i < data_count; ++i) {
    const TestURLInfo& cur(test_data[i]);
    const GURL current_url(cur.url);
    base::Time visit_time =
        base::Time::Now() - base::TimeDelta::FromDays(cur.days_from_now);
    history::URLRow url_info(current_url);
    url_info.set_id(i);
    url_info.set_title(UTF8ToUTF16(cur.title));
    url_info.set_visit_count(cur.visit_count);
    url_info.set_typed_count(cur.typed_count);
    url_info.set_last_visit(visit_time);
    url_info.set_hidden(false);
    private_data_->UpdateURL(url_info);
  }
  // Stall until the pending operations have completed.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
}

void URLIndexPrivateDataTest::ExpectPrivateDataNotEmpty(
    const URLIndexPrivateData& data) {
  EXPECT_FALSE(data.word_list_.empty());
  // available_words_ will be empty since we have freshly built the
  // data set for these tests.
  EXPECT_TRUE(data.available_words_.empty());
  EXPECT_FALSE(data.word_map_.empty());
  EXPECT_FALSE(data.char_word_map_.empty());
  EXPECT_FALSE(data.word_id_history_map_.empty());
  EXPECT_FALSE(data.history_id_word_map_.empty());
  EXPECT_FALSE(data.history_info_map_.empty());
  EXPECT_FALSE(data.word_starts_map_.empty());
}

// Helper function which compares two maps for equivalence. The maps' values
// are associative containers and their contents are compared as well.
template<typename T>
void ExpectMapOfContainersIdentical(const T& expected, const T& actual) {
  ASSERT_EQ(expected.size(), actual.size());
  for (typename T::const_iterator expected_iter = expected.begin();
       expected_iter != expected.end(); ++expected_iter) {
    typename T::const_iterator actual_iter = actual.find(expected_iter->first);
    ASSERT_TRUE(actual_iter != actual.end());
    typename T::mapped_type const& expected_values(expected_iter->second);
    typename T::mapped_type const& actual_values(actual_iter->second);
    ASSERT_EQ(expected_values.size(), actual_values.size());
    for (typename T::mapped_type::const_iterator set_iter =
         expected_values.begin(); set_iter != expected_values.end(); ++set_iter)
      EXPECT_EQ(actual_values.count(*set_iter),
                expected_values.count(*set_iter));
  }
}

void URLIndexPrivateDataTest::ExpectPrivateDataEqual(
    const URLIndexPrivateData& expected,
    const URLIndexPrivateData& actual) {
  EXPECT_EQ(expected.word_list_.size(), actual.word_list_.size());
  EXPECT_EQ(expected.word_map_.size(), actual.word_map_.size());
  EXPECT_EQ(expected.char_word_map_.size(), actual.char_word_map_.size());
  EXPECT_EQ(expected.word_id_history_map_.size(),
            actual.word_id_history_map_.size());
  EXPECT_EQ(expected.history_id_word_map_.size(),
            actual.history_id_word_map_.size());
  EXPECT_EQ(expected.history_info_map_.size(), actual.history_info_map_.size());
  EXPECT_EQ(expected.word_starts_map_.size(), actual.word_starts_map_.size());
  // WordList must be index-by-index equal.
  size_t count = expected.word_list_.size();
  for (size_t i = 0; i < count; ++i)
    EXPECT_EQ(expected.word_list_[i], actual.word_list_[i]);

  ExpectMapOfContainersIdentical(expected.char_word_map_,
                                 actual.char_word_map_);
  ExpectMapOfContainersIdentical(expected.word_id_history_map_,
                                 actual.word_id_history_map_);
  ExpectMapOfContainersIdentical(expected.history_id_word_map_,
                                 actual.history_id_word_map_);

  for (HistoryInfoMap::const_iterator expected_info =
       expected.history_info_map_.begin();
       expected_info != expected.history_info_map_.end(); ++expected_info) {
    HistoryInfoMap::const_iterator actual_info =
        actual.history_info_map_.find(expected_info->first);
    // NOTE(yfriedman): ASSERT_NE can't be used due to incompatibility between
    // gtest and STLPort in the Android build. See
    // http://code.google.com/p/googletest/issues/detail?id=359
    ASSERT_TRUE(actual_info != actual.history_info_map_.end());
    const URLRow& expected_row(expected_info->second);
    const URLRow& actual_row(actual_info->second);
    EXPECT_EQ(expected_row.visit_count(), actual_row.visit_count());
    EXPECT_EQ(expected_row.typed_count(), actual_row.typed_count());
    EXPECT_EQ(expected_row.last_visit(), actual_row.last_visit());
    EXPECT_EQ(expected_row.url(), actual_row.url());
  }

  for (WordStartsMap::const_iterator expected_starts =
       expected.word_starts_map_.begin();
       expected_starts != expected.word_starts_map_.end();
       ++expected_starts) {
    WordStartsMap::const_iterator actual_starts =
        actual.word_starts_map_.find(expected_starts->first);
    // NOTE(yfriedman): ASSERT_NE can't be used due to incompatibility between
    // gtest and STLPort in the Android build. See
    // http://code.google.com/p/googletest/issues/detail?id=359
    ASSERT_TRUE(actual_starts != actual.word_starts_map_.end());
    const RowWordStarts& expected_word_starts(expected_starts->second);
    const RowWordStarts& actual_word_starts(actual_starts->second);
    EXPECT_EQ(expected_word_starts.url_word_starts_.size(),
              actual_word_starts.url_word_starts_.size());
    EXPECT_TRUE(std::equal(expected_word_starts.url_word_starts_.begin(),
                           expected_word_starts.url_word_starts_.end(),
                           actual_word_starts.url_word_starts_.begin()));
    EXPECT_EQ(expected_word_starts.title_word_starts_.size(),
              actual_word_starts.title_word_starts_.size());
    EXPECT_TRUE(std::equal(expected_word_starts.title_word_starts_.begin(),
                           expected_word_starts.title_word_starts_.end(),
                           actual_word_starts.title_word_starts_.begin()));
  }
}

TEST_F(URLIndexPrivateDataTest, CacheFetch) {
  // Compare the in-memory index data to the on-disk cached data.
  const URLIndexPrivateData& expected_data(*(private_data_.get()));
  ExpectPrivateDataNotEmpty(expected_data);

  // Grab the index data from the cache.
  scoped_refptr<URLIndexPrivateData> cached_data = new URLIndexPrivateData;
  URLIndexPrivateData* actual_data = cached_data.get();
  ASSERT_TRUE(private_data_->cache_db_->RestorePrivateData(actual_data));
  EXPECT_TRUE(actual_data->ValidateConsistency());
  ExpectPrivateDataEqual(expected_data, *actual_data);
}

class URLIndexOldCacheTest : public testing::Test {
 public:
  URLIndexOldCacheTest();
  virtual ~URLIndexOldCacheTest();

  virtual void SetUp() OVERRIDE;

 protected:
  scoped_refptr<URLIndexPrivateData> private_data_;
  ScopedTempDir temp_cache_dir_;
};

URLIndexOldCacheTest::URLIndexOldCacheTest() {}
URLIndexOldCacheTest::~URLIndexOldCacheTest() {}

void URLIndexOldCacheTest::SetUp() {
  // Create a file that looks like an old protobuf-based cache file.
  ASSERT_TRUE(temp_cache_dir_.CreateUniqueTempDir());
  std::string dummy_data("DUMMY DATA");
  int size = dummy_data.size();
  FilePath path = temp_cache_dir_.path().Append(
                      FILE_PATH_LITERAL("History Provider Cache"));
  ASSERT_EQ(size, file_util::WriteFile(path, dummy_data.c_str(), size));
  ASSERT_TRUE(file_util::PathExists(path));

  // Continue initializing. This will attempt to restore from the SQLite cache
  // but it doesn't exist so the old protobuf file should be automatically
  // deleted.
  private_data_ = new URLIndexPrivateData(temp_cache_dir_.path(), "en");
  private_data_->Init(
      content::BrowserThread::GetBlockingPool()->GetSequenceToken());
  EXPECT_FALSE(private_data_->RestoreFromCacheTask());
}

TEST_F(URLIndexOldCacheTest, CacheProtobufDelete) {
  // If an old, protobuf-based cache file exists then it should be being deleted
  // when an attempt is made to restore the index data from the SQLite cache but
  // such SQLite cache does not exist. This will happen the first time a user
  // runs Chrome after the SQLite-based cache implementation has been added.
  // All we have to do here is verify that the file does not exist.
  FilePath path = temp_cache_dir_.path().Append(
                      FILE_PATH_LITERAL("History Provider Cache"));
  ASSERT_FALSE(file_util::PathExists(path));
}

}  // namespace history
