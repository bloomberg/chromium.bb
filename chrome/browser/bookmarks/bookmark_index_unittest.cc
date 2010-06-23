// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/message_loop.h"
#include "base/string_util.h"
#include "chrome/browser/bookmarks/bookmark_index.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_utils.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/history/history_database.h"
#include "chrome/browser/history/in_memory_database.h"
#include "chrome/browser/history/query_parser.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class BookmarkIndexTest : public testing::Test {
 public:
  BookmarkIndexTest() : model_(new BookmarkModel(NULL)) {}

  void AddBookmarksWithTitles(const wchar_t** titles, size_t count) {
    std::vector<std::wstring> title_vector;
    for (size_t i = 0; i < count; ++i)
      title_vector.push_back(titles[i]);
    AddBookmarksWithTitles(title_vector);
  }

  void AddBookmarksWithTitles(const std::vector<std::wstring>& titles) {
    GURL url("about:blank");
    for (size_t i = 0; i < titles.size(); ++i)
      model_->AddURL(model_->other_node(), static_cast<int>(i), titles[i], url);
  }

  void ExpectMatches(const std::wstring& query,
                     const wchar_t** expected_titles,
                     size_t expected_count) {
    std::vector<std::wstring> title_vector;
    for (size_t i = 0; i < expected_count; ++i)
      title_vector.push_back(expected_titles[i]);
    ExpectMatches(query, title_vector);
  }

  void ExpectMatches(const std::wstring& query,
                     const std::vector<std::wstring> expected_titles) {
    std::vector<bookmark_utils::TitleMatch> matches;
    model_->GetBookmarksWithTitlesMatching(query, 1000, &matches);
    ASSERT_EQ(expected_titles.size(), matches.size());
    for (size_t i = 0; i < expected_titles.size(); ++i) {
      bool found = false;
      for (size_t j = 0; j < matches.size(); ++j) {
        if (std::wstring(expected_titles[i]) == matches[j].node->GetTitle()) {
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
    SplitString(string, L':', &match_strings);
    for (size_t i = 0; i < match_strings.size(); ++i) {
      std::vector<std::string> chunks;
      SplitString(match_strings[i], ',', &chunks);
      ASSERT_EQ(2U, chunks.size());
      matches->push_back(Snippet::MatchPosition());
      matches->back().first = StringToInt(chunks[0]);
      matches->back().second = StringToInt(chunks[1]);
    }
  }

  void ExpectMatchPositions(const std::wstring& query,
                            const Snippet::MatchPositions& expected_positions) {
    std::vector<bookmark_utils::TitleMatch> matches;
    model_->GetBookmarksWithTitlesMatching(query, 1000, &matches);
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
    const std::wstring input;
    const std::wstring query;
    const std::wstring expected;
  } data[] = {
    // Trivial test case of only one term, exact match.
    { L"a;b",                       L"A",       L"a" },

    // Prefix match, one term.
    { L"abcd;abc;b",                L"abc",     L"abcd;abc" },

    // Prefix match, multiple terms.
    { L"abcd cdef;abcd;abcd cdefg", L"abc cde", L"abcd cdef;abcd cdefg"},

    // Exact and prefix match.
    { L"ab cdef;abcd;abcd cdefg",   L"ab cdef", L"ab cdef"},

    // Exact and prefix match.
    { L"ab cdef ghij;ab;cde;cdef;ghi;cdef ab;ghij ab",
      L"ab cde ghi",
      L"ab cdef ghij"},

    // Title with term multiple times.
    { L"ab ab",                     L"ab",      L"ab ab"},

    // Make sure quotes don't do a prefix match.
    { L"think",                     L"\"thi\"", L""},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::wstring> titles;
    SplitString(data[i].input, L';', &titles);
    AddBookmarksWithTitles(titles);

    std::vector<std::wstring> expected;
    if (!data[i].expected.empty())
      SplitString(data[i].expected, L';', &expected);

    ExpectMatches(data[i].query, expected);

    model_.reset(new BookmarkModel(NULL));
  }
}

// Makes sure match positions are updated appropriately.
TEST_F(BookmarkIndexTest, MatchPositions) {
  struct TestData {
    const std::wstring title;
    const std::wstring query;
    const std::string expected;
  } data[] = {
    // Trivial test case of only one term, exact match.
    { L"a",                         L"A",       "0,1" },
    { L"foo bar",                   L"bar",     "4,7" },
    { L"fooey bark",                L"bar foo", "0,3:6,9"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<std::wstring> titles;
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
  const wchar_t* input[] = { L"a", L"b" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  // Remove the node and make sure we don't get back any results.
  model_->Remove(model_->other_node(), 0);
  ExpectMatches(L"A", NULL, 0U);
}

// Makes sure index is updated when a node's title is changed.
TEST_F(BookmarkIndexTest, ChangeTitle) {
  const wchar_t* input[] = { L"a", L"b" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  // Remove the node and make sure we don't get back any results.
  const wchar_t* expected[] = { L"blah" };
  model_->SetTitle(model_->other_node()->GetChild(0), L"blah");
  ExpectMatches(L"BlAh", expected, ARRAYSIZE_UNSAFE(expected));
}

// Makes sure no more than max queries is returned.
TEST_F(BookmarkIndexTest, HonorMax) {
  const wchar_t* input[] = { L"abcd", L"abcde" };
  AddBookmarksWithTitles(input, ARRAYSIZE_UNSAFE(input));

  std::vector<bookmark_utils::TitleMatch> matches;
  model_->GetBookmarksWithTitlesMatching(L"ABc", 1, &matches);
  EXPECT_EQ(1U, matches.size());
}

// Makes sure if the lower case string of a bookmark title is more characters
// than the upper case string no match positions are returned.
TEST_F(BookmarkIndexTest, EmptyMatchOnMultiwideLowercaseString) {
  const BookmarkNode* n1 = model_->AddURL(model_->other_node(), 0, L"\u0130 i",
                                          GURL("http://www.google.com"));

  std::vector<bookmark_utils::TitleMatch> matches;
  model_->GetBookmarksWithTitlesMatching(L"i", 100, &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].node == n1);
  EXPECT_TRUE(matches[0].match_positions.empty());
}

TEST_F(BookmarkIndexTest, GetResultsSortedByTypedCount) {
  // This ensures MessageLoop::current() will exist, which is needed by
  // TestingProfile::BlockUntilHistoryProcessesPendingRequests().
  MessageLoop loop(MessageLoop::TYPE_DEFAULT);
  ChromeThread ui_thread(ChromeThread::UI, &loop);
  ChromeThread file_thread(ChromeThread::FILE, &loop);

  TestingProfile profile;
  profile.CreateHistoryService(true, false);
  profile.BlockUntilHistoryProcessesPendingRequests();
  profile.CreateBookmarkModel(true);
  profile.BlockUntilBookmarkModelLoaded();

  BookmarkModel* model = profile.GetBookmarkModel();

  HistoryService* const history_service =
      profile.GetHistoryService(Profile::EXPLICIT_ACCESS);

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
    model->AddURL(model->other_node(), i, UTF8ToWide(data[i].title),
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
  model->GetBookmarksWithTitlesMatching(L"google", 4, &matches);

  // The resulting order should be:
  // 1. Google (google.com) 100
  // 2. Google Reader (google.com/reader) 80
  // 3. Google Docs (docs.google.com) 50
  // 4. Google Maps (maps.google.com) 40
  EXPECT_EQ(4, static_cast<int>(matches.size()));
  EXPECT_EQ(data[0].url, matches[0].node->GetURL());
  EXPECT_EQ(data[3].url, matches[1].node->GetURL());
  EXPECT_EQ(data[2].url, matches[2].node->GetURL());
  EXPECT_EQ(data[1].url, matches[3].node->GetURL());

  matches.clear();
  // Select top two matches.
  model->GetBookmarksWithTitlesMatching(L"google", 2, &matches);

  EXPECT_EQ(2, static_cast<int>(matches.size()));
  EXPECT_EQ(data[0].url, matches[0].node->GetURL());
  EXPECT_EQ(data[3].url, matches[1].node->GetURL());
}

