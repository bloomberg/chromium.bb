// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_index.h"

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

class BookmarkIndexTest : public testing::Test {
 public:
  BookmarkIndexTest() : model_(new BookmarkModel(NULL)) {}

  void AddBookmarksWithTitles(const char** titles, size_t count) {
    std::vector<std::string> title_vector;
    for (size_t i = 0; i < count; ++i)
      title_vector.push_back(titles[i]);
    AddBookmarksWithTitles(title_vector);
  }

  void AddBookmarksWithTitles(const std::vector<std::string>& titles) {
    GURL url("about:blank");
    for (size_t i = 0; i < titles.size(); ++i)
      model_->AddURL(model_->other_node(), static_cast<int>(i),
                     ASCIIToUTF16(titles[i]), url);
  }

  void ExpectMatches(const std::string& query,
                     const char** expected_titles,
                     size_t expected_count) {
    std::vector<std::string> title_vector;
    for (size_t i = 0; i < expected_count; ++i)
      title_vector.push_back(expected_titles[i]);
    ExpectMatches(query, title_vector);
  }

  void ExpectMatches(const std::string& query,
                     const std::vector<std::string>& expected_titles) {
    std::vector<bookmark_utils::TitleMatch> matches;
    model_->GetBookmarksWithTitlesMatching(ASCIIToUTF16(query), 1000, &matches);
    ASSERT_EQ(expected_titles.size(), matches.size());
    for (size_t i = 0; i < expected_titles.size(); ++i) {
      bool found = false;
      for (size_t j = 0; j < matches.size(); ++j) {
        if (ASCIIToUTF16(expected_titles[i]) == matches[j].node->GetTitle()) {
          matches.erase(matches.begin() + j);
          found = true;
          break;
        }
      }
      ASSERT_TRUE(found);
    }
  }

  void ExtractMatchPositions(const std::string& string,
                             Snippet::MatchPositions* matches) {
    std::vector<std::string> match_strings;
    base::SplitString(string, ':', &match_strings);
    for (size_t i = 0; i < match_strings.size(); ++i) {
      std::vector<std::string> chunks;
      base::SplitString(match_strings[i], ',', &chunks);
      ASSERT_EQ(2U, chunks.size());
      matches->push_back(Snippet::MatchPosition());
      int chunks0, chunks1;
      base::StringToInt(chunks[0], &chunks0);
      base::StringToInt(chunks[1], &chunks1);
      matches->back().first = chunks0;
      matches->back().second = chunks1;
    }
  }

  void ExpectMatchPositions(const std::string& query,
                            const Snippet::MatchPositions& expected_positions) {
    std::vector<bookmark_utils::TitleMatch> matches;
    model_->GetBookmarksWithTitlesMatching(ASCIIToUTF16(query), 1000, &matches);
    ASSERT_EQ(1U, matches.size());
    const bookmark_utils::TitleMatch& match = matches[0];
    ASSERT_EQ(expected_positions.size(), match.match_positions.size());
    for (size_t i = 0; i < expected_positions.size(); ++i) {
      EXPECT_EQ(expected_positions[i].first, match.match_positions[i].first);
      EXPECT_EQ(expected_positions[i].second, match.match_positions[i].second);
    }
  }

 protected:
  scoped_ptr<BookmarkModel> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkIndexTest);
};

// Various permutations with differing input, queries and output that exercises
// all query paths.
TEST_F(BookmarkIndexTest, Tests) {
  struct TestData {
    const std::string input;
    const std::string query;
    const std::string expected;
  } data[] = {
    // Trivial test case of only one term, exact match.
    { "a;b",                        "A",        "a" },

    // Prefix match, one term.
    { "abcd;abc;b",                 "abc",      "abcd;abc" },

    // Prefix match, multiple terms.
    { "abcd cdef;abcd;abcd cdefg",  "abc cde",  "abcd cdef;abcd cdefg"},

    // Exact and prefix match.
    { "ab cdef;abcd;abcd cdefg",    "ab cdef",  "ab cdef"},

    // Exact and prefix match.
    { "ab cdef ghij;ab;cde;cdef;ghi;cdef ab;ghij ab",
      "ab cde ghi",
      "ab cdef ghij"},

    // Title with term multiple times.
    { "ab ab",                      "ab",       "ab ab"},

    // Make sure quotes don't do a prefix match.
    { "think",                      "\"thi\"",  ""},

    // Prefix matches against multiple candidates.
    { "abc1 abc2 abc3 abc4", "abc", "abc1 abc2 abc3 abc4"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::string> titles;
    base::SplitString(data[i].input, ';', &titles);
    AddBookmarksWithTitles(titles);

    std::vector<std::string> expected;
    if (!data[i].expected.empty())
      base::SplitString(data[i].expected, ';', &expected);

    ExpectMatches(data[i].query, expected);

    model_.reset(new BookmarkModel(NULL));
  }
}

// Makes sure match positions are updated appropriately.
TEST_F(BookmarkIndexTest, MatchPositions) {
  struct TestData {
    const std::string title;
    const std::string query;
    const std::string expected;
  } data[] = {
    // Trivial test case of only one term, exact match.
    { "a",                        "A",        "0,1" },
    { "foo bar",                  "bar",      "4,7" },
    { "fooey bark",               "bar foo",  "0,3:6,9"},
    // Non-trivial tests.
    { "foobar foo",               "foobar foo",   "0,6:7,10" },
    { "foobar foo",               "foo foobar",   "0,6:7,10" },
    { "foobar foobar",            "foobar foo",   "0,6:7,13" },
    { "foobar foobar",            "foo foobar",   "0,6:7,13" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::string> titles;
    titles.push_back(data[i].title);
    AddBookmarksWithTitles(titles);

    Snippet::MatchPositions expected_matches;
    ExtractMatchPositions(data[i].expected, &expected_matches);
    ExpectMatchPositions(data[i].query, expected_matches);

    model_.reset(new BookmarkModel(NULL));
  }
}

// Makes sure index is updated when a node is removed.
TEST_F(BookmarkIndexTest, Remove) {
  const char* input[] = { "a", "b" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  // Remove the node and make sure we don't get back any results.
  model_->Remove(model_->other_node(), 0);
  ExpectMatches("A", NULL, 0U);
}

// Makes sure index is updated when a node's title is changed.
TEST_F(BookmarkIndexTest, ChangeTitle) {
  const char* input[] = { "a", "b" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  // Remove the node and make sure we don't get back any results.
  const char* expected[] = { "blah" };
  model_->SetTitle(model_->other_node()->GetChild(0), ASCIIToUTF16("blah"));
  ExpectMatches("BlAh", expected, ARRAYSIZE_UNSAFE(expected));
}

// Makes sure no more than max queries is returned.
TEST_F(BookmarkIndexTest, HonorMax) {
  const char* input[] = { "abcd", "abcde" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  std::vector<bookmark_utils::TitleMatch> matches;
  model_->GetBookmarksWithTitlesMatching(ASCIIToUTF16("ABc"), 1, &matches);
  EXPECT_EQ(1U, matches.size());
}

// Makes sure if the lower case string of a bookmark title is more characters
// than the upper case string no match positions are returned.
TEST_F(BookmarkIndexTest, EmptyMatchOnMultiwideLowercaseString) {
  const BookmarkNode* n1 = model_->AddURL(model_->other_node(), 0,
                                          base::WideToUTF16(L"\u0130 i"),
                                          GURL("http://www.google.com"));

  std::vector<bookmark_utils::TitleMatch> matches;
  model_->GetBookmarksWithTitlesMatching(ASCIIToUTF16("i"), 100, &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].node == n1);
  EXPECT_TRUE(matches[0].match_positions.empty());
}

TEST_F(BookmarkIndexTest, GetResultsSortedByTypedCount) {
  // This ensures MessageLoop::current() will exist, which is needed by
  // TestingProfile::BlockUntilHistoryProcessesPendingRequests().
  MessageLoop loop(MessageLoop::TYPE_DEFAULT);
  content::TestBrowserThread ui_thread(BrowserThread::UI, &loop);
  content::TestBrowserThread file_thread(BrowserThread::FILE, &loop);

  TestingProfile profile;
  profile.CreateHistoryService(true, false);
  profile.BlockUntilHistoryProcessesPendingRequests();
  profile.CreateBookmarkModel(true);
  profile.BlockUntilBookmarkModelLoaded();

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);

  HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(&profile, Profile::EXPLICIT_ACCESS);

  history::URLDatabase* url_db = history_service->InMemoryDatabase();

  struct TestData {
    const GURL url;
    const char* title;
    const int typed_count;
  } data[] = {
    { GURL("http://www.google.com/"),      "Google",           100 },
    { GURL("http://maps.google.com/"),     "Google Maps",       40 },
    { GURL("http://docs.google.com/"),     "Google Docs",       50 },
    { GURL("http://reader.google.com/"),   "Google Reader",     80 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    history::URLRow info(data[i].url);
    info.set_title(UTF8ToUTF16(data[i].title));
    info.set_typed_count(data[i].typed_count);
    // Populate the InMemoryDatabase....
    url_db->AddURL(info);
    // Populate the BookmarkIndex.
    model->AddURL(model->other_node(), i, UTF8ToUTF16(data[i].title),
                  data[i].url);
  }

  // Check that the InMemoryDatabase stored the URLs properly.
  history::URLRow result1;
  url_db->GetRowForURL(data[0].url, &result1);
  EXPECT_EQ(data[0].title, UTF16ToUTF8(result1.title()));

  history::URLRow result2;
  url_db->GetRowForURL(data[1].url, &result2);
  EXPECT_EQ(data[1].title, UTF16ToUTF8(result2.title()));

  history::URLRow result3;
  url_db->GetRowForURL(data[2].url, &result3);
  EXPECT_EQ(data[2].title, UTF16ToUTF8(result3.title()));

  history::URLRow result4;
  url_db->GetRowForURL(data[3].url, &result4);
  EXPECT_EQ(data[3].title, UTF16ToUTF8(result4.title()));

  // Populate match nodes.
  std::vector<bookmark_utils::TitleMatch> matches;
  model->GetBookmarksWithTitlesMatching(ASCIIToUTF16("google"), 4, &matches);

  // The resulting order should be:
  // 1. Google (google.com) 100
  // 2. Google Reader (google.com/reader) 80
  // 3. Google Docs (docs.google.com) 50
  // 4. Google Maps (maps.google.com) 40
  EXPECT_EQ(4, static_cast<int>(matches.size()));
  EXPECT_EQ(data[0].url, matches[0].node->url());
  EXPECT_EQ(data[3].url, matches[1].node->url());
  EXPECT_EQ(data[2].url, matches[2].node->url());
  EXPECT_EQ(data[1].url, matches[3].node->url());

  matches.clear();
  // Select top two matches.
  model->GetBookmarksWithTitlesMatching(ASCIIToUTF16("google"), 2, &matches);

  EXPECT_EQ(2, static_cast<int>(matches.size()));
  EXPECT_EQ(data[0].url, matches[0].node->url());
  EXPECT_EQ(data[3].url, matches[1].node->url());
}
