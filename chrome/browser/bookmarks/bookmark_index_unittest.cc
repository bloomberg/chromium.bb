// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_index.h"

#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_test_helpers.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history/url_database.h"
#include "chrome/test/base/testing_profile.h"
#include "components/bookmarks/core/browser/bookmark_match.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

class BookmarkIndexTest : public testing::Test {
 public:
  BookmarkIndexTest() : model_(new BookmarkModel(NULL, false)) {
  }

  typedef std::pair<std::string, std::string> TitleAndURL;

  void AddBookmarks(const char** titles, const char** urls, size_t count) {
    // The pair is (title, url).
    std::vector<TitleAndURL> bookmarks;
    for (size_t i = 0; i < count; ++i) {
      TitleAndURL bookmark(titles[i], urls[i]);
      bookmarks.push_back(bookmark);
    }
    AddBookmarks(bookmarks);
  }

  void AddBookmarks(const std::vector<TitleAndURL> bookmarks) {
    for (size_t i = 0; i < bookmarks.size(); ++i) {
      model_->AddURL(model_->other_node(), static_cast<int>(i),
                     ASCIIToUTF16(bookmarks[i].first),
                     GURL(bookmarks[i].second));
    }
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
    std::vector<BookmarkMatch> matches;
    model_->GetBookmarksMatching(ASCIIToUTF16(query), 1000, &matches);
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
                             BookmarkMatch::MatchPositions* matches) {
    std::vector<std::string> match_strings;
    base::SplitString(string, ':', &match_strings);
    for (size_t i = 0; i < match_strings.size(); ++i) {
      std::vector<std::string> chunks;
      base::SplitString(match_strings[i], ',', &chunks);
      ASSERT_EQ(2U, chunks.size());
      matches->push_back(BookmarkMatch::MatchPosition());
      int chunks0, chunks1;
      base::StringToInt(chunks[0], &chunks0);
      base::StringToInt(chunks[1], &chunks1);
      matches->back().first = chunks0;
      matches->back().second = chunks1;
    }
  }

  void ExpectMatchPositions(
      const BookmarkMatch::MatchPositions& actual_positions,
      const BookmarkMatch::MatchPositions& expected_positions) {
    ASSERT_EQ(expected_positions.size(), actual_positions.size());
    for (size_t i = 0; i < expected_positions.size(); ++i) {
      EXPECT_EQ(expected_positions[i].first, actual_positions[i].first);
      EXPECT_EQ(expected_positions[i].second, actual_positions[i].second);
    }
  }

 protected:
  scoped_ptr<BookmarkModel> model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkIndexTest);
};

// Various permutations with differing input, queries and output that exercises
// all query paths.
TEST_F(BookmarkIndexTest, GetBookmarksMatching) {
  struct TestData {
    const std::string titles;
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
    base::SplitString(data[i].titles, ';', &titles);
    std::vector<TitleAndURL> bookmarks;
    for (size_t j = 0; j < titles.size(); ++j) {
      TitleAndURL bookmark(titles[j], "about:blank");
      bookmarks.push_back(bookmark);
    }
    AddBookmarks(bookmarks);

    std::vector<std::string> expected;
    if (!data[i].expected.empty())
      base::SplitString(data[i].expected, ';', &expected);

    ExpectMatches(data[i].query, expected);

    model_.reset(new BookmarkModel(NULL, false));
  }
}

// Analogous to GetBookmarksMatching, this test tests various permutations
// of title, URL, and input to see if the title/URL matches the input as
// expected.
TEST_F(BookmarkIndexTest, GetBookmarksMatchingWithURLs) {
  struct TestData {
    const std::string query;
    const std::string title;
    const std::string url;
    const bool should_be_retrieved;
  } data[] = {
    // Test single-word inputs.  Include both exact matches and prefix matches.
    { "foo", "Foo",    "http://www.bar.com/",    true  },
    { "foo", "Foodie", "http://www.bar.com/",    true  },
    { "foo", "Bar",    "http://www.foo.com/",    true  },
    { "foo", "Bar",    "http://www.foodie.com/", true  },
    { "foo", "Foo",    "http://www.foo.com/",    true  },
    { "foo", "Bar",    "http://www.bar.com/",    false },
    { "foo", "Bar",    "http://www.bar.com/blah/foo/blah-again/ ",    true  },
    { "foo", "Bar",    "http://www.bar.com/blah/foodie/blah-again/ ", true  },
    { "foo", "Bar",    "http://www.bar.com/blah-foo/blah-again/ ",    true  },
    { "foo", "Bar",    "http://www.bar.com/blah-foodie/blah-again/ ", true  },
    { "foo", "Bar",    "http://www.bar.com/blahafoo/blah-again/ ",    false },

    // Test multi-word inputs.
    { "foo bar", "Foo Bar",      "http://baz.com/",   true  },
    { "foo bar", "Foodie Bar",   "http://baz.com/",   true  },
    { "bar foo", "Foo Bar",      "http://baz.com/",   true  },
    { "bar foo", "Foodie Barly", "http://baz.com/",   true  },
    { "foo bar", "Foo Baz",      "http://baz.com/",   false },
    { "foo bar", "Foo Baz",      "http://bar.com/",   true  },
    { "foo bar", "Foo Baz",      "http://barly.com/", true  },
    { "foo bar", "Foodie Baz",   "http://barly.com/", true  },
    { "bar foo", "Foo Baz",      "http://bar.com/",   true  },
    { "bar foo", "Foo Baz",      "http://barly.com/", true  },
    { "foo bar", "Baz Bar",      "http://blah.com/foo",         true  },
    { "foo bar", "Baz Barly",    "http://blah.com/foodie",      true  },
    { "foo bar", "Baz Bur",      "http://blah.com/foo/bar",     true  },
    { "foo bar", "Baz Bur",      "http://blah.com/food/barly",  true  },
    { "foo bar", "Baz Bur",      "http://bar.com/blah/foo",     true  },
    { "foo bar", "Baz Bur",      "http://barly.com/blah/food",  true  },
    { "foo bar", "Baz Bur",      "http://bar.com/blah/flub",    false },
    { "foo bar", "Baz Bur",      "http://foo.com/blah/flub",    false }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    model_.reset(new BookmarkModel(NULL, true));
    std::vector<TitleAndURL> bookmarks;
    bookmarks.push_back(TitleAndURL(data[i].title, data[i].url));
    AddBookmarks(bookmarks);

    std::vector<std::string> expected;
    if (data[i].should_be_retrieved)
      expected.push_back(data[i].title);

    ExpectMatches(data[i].query, expected);
  }
}

TEST_F(BookmarkIndexTest, Normalization) {
  struct TestData {
    const char* title;
    const char* query;
  } data[] = {
    { "fooa\xcc\x88-test", "foo\xc3\xa4-test" },
    { "fooa\xcc\x88-test", "fooa\xcc\x88-test" },
    { "fooa\xcc\x88-test", "foo\xc3\xa4" },
    { "fooa\xcc\x88-test", "fooa\xcc\x88" },
    { "fooa\xcc\x88-test", "foo" },
    { "foo\xc3\xa4-test", "foo\xc3\xa4-test" },
    { "foo\xc3\xa4-test", "fooa\xcc\x88-test" },
    { "foo\xc3\xa4-test", "foo\xc3\xa4" },
    { "foo\xc3\xa4-test", "fooa\xcc\x88" },
    { "foo\xc3\xa4-test", "foo" },
    { "foo", "foo" }
  };

  GURL url("about:blank");
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    model_->AddURL(model_->other_node(), 0, base::UTF8ToUTF16(data[i].title),
                   url);
    std::vector<BookmarkMatch> matches;
    model_->GetBookmarksMatching(
        base::UTF8ToUTF16(data[i].query), 10, &matches);
    EXPECT_EQ(1u, matches.size());
    model_.reset(new BookmarkModel(NULL, false));
  }
}

// Makes sure match positions are updated appropriately for title matches.
TEST_F(BookmarkIndexTest, MatchPositionsTitles) {
  struct TestData {
    const std::string title;
    const std::string query;
    const std::string expected_title_match_positions;
  } data[] = {
    // Trivial test case of only one term, exact match.
    { "a",                        "A",        "0,1" },
    { "foo bar",                  "bar",      "4,7" },
    { "fooey bark",               "bar foo",  "0,3:6,9" },
    // Non-trivial tests.
    { "foobar foo",               "foobar foo",   "0,6:7,10" },
    { "foobar foo",               "foo foobar",   "0,6:7,10" },
    { "foobar foobar",            "foobar foo",   "0,6:7,13" },
    { "foobar foobar",            "foo foobar",   "0,6:7,13" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    std::vector<TitleAndURL> bookmarks;
    TitleAndURL bookmark(data[i].title, "about:blank");
    bookmarks.push_back(bookmark);
    AddBookmarks(bookmarks);

    std::vector<BookmarkMatch> matches;
    model_->GetBookmarksMatching(ASCIIToUTF16(data[i].query), 1000, &matches);
    ASSERT_EQ(1U, matches.size());

    BookmarkMatch::MatchPositions expected_title_matches;
    ExtractMatchPositions(data[i].expected_title_match_positions,
                          &expected_title_matches);
    ExpectMatchPositions(matches[0].title_match_positions,
                         expected_title_matches);

    model_.reset(new BookmarkModel(NULL, false));
  }
}

// Makes sure match positions are updated appropriately for URL matches.
TEST_F(BookmarkIndexTest, MatchPositionsURLs) {
  struct TestData {
    const std::string query;
    const std::string url;
    const std::string expected_url_match_positions;
  } data[] = {
    { "foo",      "http://www.foo.com/",    "11,14" },
    { "foo",      "http://www.foodie.com/", "11,14" },
    { "foo",      "http://www.foofoo.com/", "11,14" },
    { "www",      "http://www.foo.com/",    "7,10"  },
    { "foo",      "http://www.foodie.com/blah/foo/fi", "11,14:27,30"      },
    { "foo",      "http://www.blah.com/blah/foo/fi",   "25,28"            },
    { "foo www",  "http://www.foodie.com/blah/foo/fi", "7,10:11,14:27,30" },
    { "www foo",  "http://www.foodie.com/blah/foo/fi", "7,10:11,14:27,30" },
    { "www bla",  "http://www.foodie.com/blah/foo/fi", "7,10:22,25"       },
    { "http",     "http://www.foo.com/",               "0,4"              },
    { "http www", "http://www.foo.com/",               "0,4:7,10"         },
    { "http foo", "http://www.foo.com/",               "0,4:11,14"        },
    { "http foo", "http://www.bar.com/baz/foodie/hi",  "0,4:23,26"        }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(data); ++i) {
    model_.reset(new BookmarkModel(NULL, true));
    std::vector<TitleAndURL> bookmarks;
    TitleAndURL bookmark("123456", data[i].url);
    bookmarks.push_back(bookmark);
    AddBookmarks(bookmarks);

    std::vector<BookmarkMatch> matches;
    model_->GetBookmarksMatching(ASCIIToUTF16(data[i].query), 1000, &matches);
    ASSERT_EQ(1U, matches.size()) << data[i].url << data[i].query;

    BookmarkMatch::MatchPositions expected_url_matches;
    ExtractMatchPositions(data[i].expected_url_match_positions,
                          &expected_url_matches);
    ExpectMatchPositions(matches[0].url_match_positions, expected_url_matches);
  }
}

// Makes sure index is updated when a node is removed.
TEST_F(BookmarkIndexTest, Remove) {
  const char* titles[] = { "a", "b" };
  const char* urls[] = { "about:blank", "about:blank" };
  AddBookmarks(titles, urls, ARRAYSIZE_UNSAFE(titles));

  // Remove the node and make sure we don't get back any results.
  model_->Remove(model_->other_node(), 0);
  ExpectMatches("A", NULL, 0U);
}

// Makes sure index is updated when a node's title is changed.
TEST_F(BookmarkIndexTest, ChangeTitle) {
  const char* titles[] = { "a", "b" };
  const char* urls[] = { "about:blank", "about:blank" };
  AddBookmarks(titles, urls, ARRAYSIZE_UNSAFE(titles));

  // Remove the node and make sure we don't get back any results.
  const char* expected[] = { "blah" };
  model_->SetTitle(model_->other_node()->GetChild(0), ASCIIToUTF16("blah"));
  ExpectMatches("BlAh", expected, ARRAYSIZE_UNSAFE(expected));
}

// Makes sure no more than max queries is returned.
TEST_F(BookmarkIndexTest, HonorMax) {
  const char* titles[] = { "abcd", "abcde" };
  const char* urls[] = { "about:blank", "about:blank" };
  AddBookmarks(titles, urls, ARRAYSIZE_UNSAFE(titles));

  std::vector<BookmarkMatch> matches;
  model_->GetBookmarksMatching(ASCIIToUTF16("ABc"), 1, &matches);
  EXPECT_EQ(1U, matches.size());
}

// Makes sure if the lower case string of a bookmark title is more characters
// than the upper case string no match positions are returned.
TEST_F(BookmarkIndexTest, EmptyMatchOnMultiwideLowercaseString) {
  const BookmarkNode* n1 = model_->AddURL(model_->other_node(), 0,
                                          base::WideToUTF16(L"\u0130 i"),
                                          GURL("http://www.google.com"));

  std::vector<BookmarkMatch> matches;
  model_->GetBookmarksMatching(ASCIIToUTF16("i"), 100, &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_TRUE(matches[0].node == n1);
  EXPECT_TRUE(matches[0].title_match_positions.empty());
}

TEST_F(BookmarkIndexTest, GetResultsSortedByTypedCount) {
  // This ensures MessageLoop::current() will exist, which is needed by
  // TestingProfile::BlockUntilHistoryProcessesPendingRequests().
  content::TestBrowserThreadBundle thread_bundle;

  TestingProfile profile;
  ASSERT_TRUE(profile.CreateHistoryService(true, false));
  profile.BlockUntilHistoryProcessesPendingRequests();
  profile.CreateBookmarkModel(true);

  BookmarkModel* model = BookmarkModelFactory::GetForProfile(&profile);
  test::WaitForBookmarkModelToLoad(model);

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
    info.set_title(base::UTF8ToUTF16(data[i].title));
    info.set_typed_count(data[i].typed_count);
    // Populate the InMemoryDatabase....
    url_db->AddURL(info);
    // Populate the BookmarkIndex.
    model->AddURL(model->other_node(), i, base::UTF8ToUTF16(data[i].title),
                  data[i].url);
  }

  // Check that the InMemoryDatabase stored the URLs properly.
  history::URLRow result1;
  url_db->GetRowForURL(data[0].url, &result1);
  EXPECT_EQ(data[0].title, base::UTF16ToUTF8(result1.title()));

  history::URLRow result2;
  url_db->GetRowForURL(data[1].url, &result2);
  EXPECT_EQ(data[1].title, base::UTF16ToUTF8(result2.title()));

  history::URLRow result3;
  url_db->GetRowForURL(data[2].url, &result3);
  EXPECT_EQ(data[2].title, base::UTF16ToUTF8(result3.title()));

  history::URLRow result4;
  url_db->GetRowForURL(data[3].url, &result4);
  EXPECT_EQ(data[3].title, base::UTF16ToUTF8(result4.title()));

  // Populate match nodes.
  std::vector<BookmarkMatch> matches;
  model->GetBookmarksMatching(ASCIIToUTF16("google"), 4, &matches);

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
  model->GetBookmarksMatching(ASCIIToUTF16("google"), 2, &matches);

  EXPECT_EQ(2, static_cast<int>(matches.size()));
  EXPECT_EQ(data[0].url, matches[0].node->url());
  EXPECT_EQ(data[3].url, matches[1].node->url());
}
