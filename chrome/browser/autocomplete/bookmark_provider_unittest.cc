// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/bookmark_provider.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete/autocomplete_provider.h"
#include "chrome/browser/autocomplete/autocomplete_provider_listener.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

// The bookmark corpus against which we will simulate searches.
struct BookmarksTestInfo {
  std::string title;
  std::string url;
} bookmark_provider_test_data[] = {
  { "abc def", "http://www.catsanddogs.com/a" },
  { "abcde", "http://www.catsanddogs.com/b" },
  { "abcdef", "http://www.catsanddogs.com/c" },
  { "a definition", "http://www.catsanddogs.com/d" },
  { "carry carbon carefully", "http://www.catsanddogs.com/e" },
  { "ghi jkl", "http://www.catsanddogs.com/f" },
  { "jkl ghi", "http://www.catsanddogs.com/g" },
  { "frankly frankly frank", "http://www.catsanddogs.com/h" },
  { "foobar foobar", "http://www.foobar.com/" },
  // For testing ranking with different URLs.
  {"achlorhydric featherheads resuscitates mockingbirds",
   "http://www.featherheads.com/a" },
  {"achlorhydric mockingbirds resuscitates featherhead",
   "http://www.featherheads.com/b" },
  {"featherhead resuscitates achlorhydric mockingbirds",
   "http://www.featherheads.com/c" },
  {"mockingbirds resuscitates featherheads achlorhydric",
   "http://www.featherheads.com/d" },
  // For testing URL boosting.
  {"burning worms #1", "http://www.burned.com/" },
  {"burning worms #2", "http://www.worms.com/" },
  {"worming burns #10", "http://www.burned.com/" },
  {"worming burns #20", "http://www.worms.com/" },
  {"jive music", "http://www.worms.com/" },
};

class BookmarkProviderTest : public testing::Test,
                             public AutocompleteProviderListener {
 public:
  BookmarkProviderTest() : model_(new BookmarkModel(NULL)) {}

  // AutocompleteProviderListener: Not called.
  virtual void OnProviderUpdate(bool updated_matches) OVERRIDE {}

 protected:
  virtual void SetUp() OVERRIDE;

  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<BookmarkModel> model_;
  scoped_refptr<BookmarkProvider> provider_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BookmarkProviderTest);
};

void BookmarkProviderTest::SetUp() {
  profile_.reset(new TestingProfile());
  DCHECK(profile_.get());
  provider_ = new BookmarkProvider(this, profile_.get());
  DCHECK(provider_);
  provider_->set_bookmark_model_for_testing(model_.get());

  const BookmarkNode* other_node = model_->other_node();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(bookmark_provider_test_data); ++i) {
    const BookmarksTestInfo& cur(bookmark_provider_test_data[i]);
    const GURL url(cur.url);
    model_->AddURL(other_node, other_node->child_count(),
                   ASCIIToUTF16(cur.title), url);
  }
}

// Structures and functions supporting the BookmarkProviderTest.Positions
// unit test.

struct TestBookmarkPosition {
  TestBookmarkPosition(size_t begin, size_t end)
      : begin(begin), end(end) {}

  size_t begin;
  size_t end;
};
typedef std::vector<TestBookmarkPosition> TestBookmarkPositions;

// Return |positions| as a formatted string for unit test diagnostic output.
std::string TestBookmarkPositionsAsString(
    const TestBookmarkPositions& positions) {
  std::string position_string("{");
  for (TestBookmarkPositions::const_iterator i = positions.begin();
       i != positions.end(); ++i) {
    if (i != positions.begin())
      position_string += ", ";
    position_string += "{" + base::IntToString(i->begin) + ", " +
        base::IntToString(i->end) + "}";
  }
  position_string += "}\n";
  return position_string;
}

// Return the positions in |matches| as a formatted string for unit test
// diagnostic output.
string16 MatchesAsString16(const ACMatches& matches) {
  string16 matches_string;
  for (ACMatches::const_iterator i = matches.begin(); i != matches.end(); ++i) {
    matches_string.append(ASCIIToUTF16("    '"));
    matches_string.append(i->description);
    matches_string.append(ASCIIToUTF16("'\n"));
  }
  return matches_string;
}

// Comparison function for sorting search terms by descending length.
bool TestBookmarkPositionsEqual(const TestBookmarkPosition& pos_a,
                                const TestBookmarkPosition& pos_b) {
  return pos_a.begin == pos_b.begin && pos_a.end == pos_b.end;
}

// Convience function to make comparing ACMatchClassifications against the
// test expectations structure easier.
TestBookmarkPositions PositionsFromAutocompleteMatch(
    const AutocompleteMatch& match) {
  TestBookmarkPositions positions;
  bool started = false;
  size_t start = 0;
  for (AutocompleteMatch::ACMatchClassifications::const_iterator
       i = match.description_class.begin();
       i != match.description_class.end(); ++i) {
    if (i->style & AutocompleteMatch::ACMatchClassification::MATCH) {
      // We have found the start of a match.
      EXPECT_FALSE(started);
      started = true;
      start = i->offset;
    } else if (started) {
      // We have found the end of a match.
      started = false;
      positions.push_back(TestBookmarkPosition(start, i->offset));
      start = 0;
    }
  }
  // Record the final position if the last match goes to the end of the
  // candidate string.
  if (started)
    positions.push_back(TestBookmarkPosition(start, match.description.size()));
  return positions;
}

// Convience function to make comparing test expectations structure against the
// actual ACMatchClassifications easier.
TestBookmarkPositions PositionsFromExpectations(
    const size_t expectations[9][2]) {
  TestBookmarkPositions positions;
  size_t i = 0;
  // The array is zero-terminated in the [1]th element.
  while (expectations[i][1]) {
    positions.push_back(
        TestBookmarkPosition(expectations[i][0], expectations[i][1]));
    ++i;
  }
  return positions;
}

TEST_F(BookmarkProviderTest, Positions) {
  // Simulate searches.
  // Description of |positions|:
  //   The first index represents the collection of positions for each expected
  //   match. The count of the actual subarrays in each instance of |query_data|
  //   must equal |match_count|. The second index represents each expected
  //   match position. The third index represents the |start| and |end| of the
  //   expected match's position within the |test_data|. This array must be
  //   terminated by an entry with a value of '0' for |end|.
  // Example:
  //   Consider the line for 'def' below:
  //     {"def", 2, {{{4, 7}, {XXX, 0}}, {{2, 5}, {11, 14}, {XXX, 0}}}},
  //   There are two expected matches:
  //     0. {{4, 7}, {XXX, 0}}
  //     1. {{2, 5}, {11 ,14}, {XXX, 0}}
  //   For the first match, [0], there is one match within the bookmark's title
  //   expected, {4, 7}, which maps to the 'def' within "abc def". The 'XXX'
  //   value is ignored. The second match, [1], indicates that two matches are
  //   expected within the bookmark title "a definite definition". In each case,
  //   the {XXX, 0} indicates the end of the subarray. Or:
  //                 Match #1            Match #2
  //                 ------------------  ----------------------------
  //                  Pos1    Term        Pos1    Pos2      Term
  //                  ------  --------    ------  --------  --------
  //     {"def", 2, {{{4, 7}, {999, 0}}, {{2, 5}, {11, 14}, {999, 0}}}},
  //
  struct QueryData {
    const std::string query;
    const size_t match_count;  // This count must match the number of major
                               // elements in the following |positions| array.
    const size_t positions[99][9][2];
  } query_data[] = {
    // This first set is primarily for position detection validation.
    {"abc",                   3, {{{0, 3}, {0, 0}},
                                  {{0, 3}, {0, 0}},
                                  {{0, 3}, {0, 0}}}},
    {"abcde",                 2, {{{0, 5}, {0, 0}},
                                  {{0, 5}, {0, 0}}}},
    {"foo bar",               0, {{{0, 0}}}},
    {"fooey bark",            0, {{{0, 0}}}},
    {"def",                   2, {{{2, 5}, {0, 0}},
                                  {{4, 7}, {0, 0}}}},
    {"ghi jkl",               2, {{{0, 3}, {4, 7}, {0, 0}},
                                  {{0, 3}, {4, 7}, {0, 0}}}},
    // NB: GetBookmarksWithTitlesMatching(...) uses exact match for "a".
    {"a",                     1, {{{0, 1}, {0, 0}}}},
    {"a d",                   0, {{{0, 0}}}},
    {"carry carbon",          1, {{{0, 5}, {6, 12}, {0, 0}}}},
    // NB: GetBookmarksWithTitlesMatching(...) sorts the match positions.
    {"carbon carry",          1, {{{0, 5}, {6, 12}, {0, 0}}}},
    {"arbon",                 0, {{{0, 0}}}},
    {"ar",                    0, {{{0, 0}}}},
    {"arry",                  0, {{{0, 0}}}},
    // Quoted terms are single terms.
    {"\"carry carbon\"",      1, {{{0, 12}, {0, 0}}}},
    {"\"carry carbon\" care", 1, {{{0, 12}, {13, 17}, {0, 0}}}},
    // Quoted terms require complete word matches.
    {"\"carry carbo\"",       0, {{{0, 0}}}},
    // This set uses duplicated and/or overlaps search terms in the title.
    {"frank",                 1, {{{0, 5}, {8, 13}, {16, 21}, {0, 0}}}},
    {"frankly",               1, {{{0, 7}, {8, 15}, {0, 0}}}},
    {"frankly frankly",       1, {{{0, 7}, {8, 15}, {0, 0}}}},
    {"foobar foo",            1, {{{0, 6}, {7, 13}, {0, 0}}}},
    {"foo foobar",            1, {{{0, 6}, {7, 13}, {0, 0}}}},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(query_data); ++i) {
    AutocompleteInput input(ASCIIToUTF16(query_data[i].query),
                            string16::npos, string16(), GURL(), false, false,
                            false, AutocompleteInput::ALL_MATCHES);
    provider_->Start(input, false);
    const ACMatches& matches(provider_->matches());
    // Validate number of results is as expected.
    EXPECT_LE(matches.size(), query_data[i].match_count)
        << "One or more of the following matches were unexpected:\n"
        << MatchesAsString16(matches)
        << "For query '" << query_data[i].query << "'.";
    EXPECT_GE(matches.size(), query_data[i].match_count)
        << "One or more expected matches are missing. Matches found:\n"
        << MatchesAsString16(matches)
        << "for query '" << query_data[i].query << "'.";
    // Validate positions within each match is as expected.
    for (size_t j = 0; j < matches.size(); ++j) {
      // Collect the expected positions as a vector, collect the match's
      // classifications for match positions as a vector, then compare.
      TestBookmarkPositions expected_positions(
          PositionsFromExpectations(query_data[i].positions[j]));
      TestBookmarkPositions actual_positions(
          PositionsFromAutocompleteMatch(matches[j]));
      EXPECT_TRUE(std::equal(expected_positions.begin(),
                             expected_positions.end(),
                             actual_positions.begin(),
                             TestBookmarkPositionsEqual))
          << "EXPECTED: " << TestBookmarkPositionsAsString(expected_positions)
          << "ACTUAL:   " << TestBookmarkPositionsAsString(actual_positions)
          << "    for query: '" << query_data[i].query << "'.";
    }
  }
}

TEST_F(BookmarkProviderTest, Rankings) {
  // Simulate searches.
  struct QueryData {
    const std::string query;
    // |match_count| must match the number of elements in the following
    // |matches| array.
    const size_t match_count;
    // |matches| specifies the titles for all bookmarks expected to be matched
    // by the |query|
    const std::string matches[99];
  } query_data[] = {
    // Basic ranking test.
    {"abc",       3, {"abcde",      // Most complete match.
                      "abcdef",
                      "abc def"}},  // Least complete match.
    {"ghi",       2, {"ghi jkl",    // Matched earlier.
                      "jkl ghi"}},  // Matched later.
    // Rankings of exact-word matches with different URLs.
    {"achlorhydric",
                  3, {"achlorhydric mockingbirds resuscitates featherhead",
                      "achlorhydric featherheads resuscitates mockingbirds",
                      "featherhead resuscitates achlorhydric mockingbirds"}},
    {"achlorhydric featherheads",
                  2, {"achlorhydric featherheads resuscitates mockingbirds",
                      "mockingbirds resuscitates featherheads achlorhydric"}},
    {"mockingbirds resuscitates",
                  3, {"mockingbirds resuscitates featherheads achlorhydric",
                      "achlorhydric mockingbirds resuscitates featherhead",
                      "featherhead resuscitates achlorhydric mockingbirds"}},
    // Ranking of exact-word matches with URL boost.
    {"worms",     2, {"burning worms #2",    // boosted
                      "burning worms #1"}},  // not boosted
    // Ranking of prefix matches with URL boost. Note that a query of
    // "worm burn" will have the same results.
    {"burn worm", 3, {"burning worms #2",    // boosted
                      "worming burns #20",   // boosted
                      "burning worms #1"}},  // not boosted but shorter
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(query_data); ++i) {
    AutocompleteInput input(ASCIIToUTF16(query_data[i].query),
                            string16::npos, string16(), GURL(), false, false,
                            false, AutocompleteInput::ALL_MATCHES);
    provider_->Start(input, false);
    const ACMatches& matches(provider_->matches());
    // Validate number and content of results is as expected.
    for (size_t j = 0; j < std::max(query_data[i].match_count, matches.size());
         ++j) {
      EXPECT_LT(j, query_data[i].match_count) << "    Unexpected match '"
          << UTF16ToUTF8(matches[j].description) << "' for query: '"
          <<  query_data[i].query << "'.";
      if (j >= query_data[i].match_count)
        continue;
      EXPECT_LT(j, matches.size()) << "    Missing match '"
          << query_data[i].matches[j] << "' for query: '"
          << query_data[i].query << "'.";
      if (j >= matches.size())
        continue;
      EXPECT_EQ(query_data[i].matches[j], UTF16ToUTF8(matches[j].description))
          << "    Mismatch at [" << base::IntToString(j) << "] for query '"
          << query_data[i].query << "'.";
    }
  }
}
