// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/titled_url_match_utils.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/bookmarks/browser/titled_url_match.h"
#include "components/bookmarks/browser/titled_url_node.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/autocomplete_provider.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "url/gurl.h"

using bookmarks::TitledUrlMatchToAutocompleteMatch;
using bookmarks::CorrectTitleAndMatchPositions;

namespace {

// A simple AutocompleteProvider that does nothing.
class MockAutocompleteProvider : public AutocompleteProvider {
 public:
  MockAutocompleteProvider(Type type) : AutocompleteProvider(type) {}

  void Start(const AutocompleteInput& input, bool minimal_changes) override {}

 private:
  ~MockAutocompleteProvider() override {}
};

class MockTitledUrlNode : public bookmarks::TitledUrlNode {
 public:
  MockTitledUrlNode(const base::string16& title, const GURL& url)
      : title_(title), url_(url) {}

  // TitledUrlNode
  const base::string16& GetTitledUrlNodeTitle() const override {
    return title_;
  }
  const GURL& GetTitledUrlNodeUrl() const override { return url_; }

 private:
  base::string16 title_;
  GURL url_;
};

}  // namespace

bool operator==(const ACMatchClassification& lhs,
                const ACMatchClassification& rhs) {
  return (lhs.offset == rhs.offset) && (lhs.style == rhs.style);
}

TEST(TitledUrlMatchUtilsTest, TitledUrlMatchToAutocompleteMatch) {
  base::string16 input_text(base::ASCIIToUTF16("goo"));
  base::string16 match_title(base::ASCIIToUTF16("Google Search"));
  GURL match_url("https://www.google.com/");
  AutocompleteMatchType::Type type = AutocompleteMatchType::BOOKMARK_TITLE;
  int relevance = 123;

  MockTitledUrlNode node(match_title, match_url);
  bookmarks::TitledUrlMatch titled_url_match;
  titled_url_match.node = &node;
  titled_url_match.title_match_positions = {{0, 3}};
  titled_url_match.url_match_positions = {{12, 15}};

  scoped_refptr<MockAutocompleteProvider> provider =
      new MockAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  TestSchemeClassifier classifier;
  AutocompleteInput input(input_text, metrics::OmniboxEventProto::NTP,
                          classifier);
  const base::string16 fixed_up_input(input_text);

  AutocompleteMatch autocomplete_match = TitledUrlMatchToAutocompleteMatch(
      titled_url_match, type, relevance, provider.get(), classifier, input,
      fixed_up_input);

  ACMatchClassifications expected_contents_class = {
      {0, ACMatchClassification::URL},
      {12, ACMatchClassification::URL | ACMatchClassification::MATCH},
      {15, ACMatchClassification::URL},
  };
  ACMatchClassifications expected_description_class = {
      {0, ACMatchClassification::MATCH}, {3, ACMatchClassification::NONE},
  };
  base::string16 expected_inline_autocompletion(base::ASCIIToUTF16("gle.com"));
  base::string16 expected_contents(
      base::ASCIIToUTF16("https://www.google.com"));

  EXPECT_EQ(provider.get(), autocomplete_match.provider);
  EXPECT_EQ(type, autocomplete_match.type);
  EXPECT_EQ(relevance, autocomplete_match.relevance);
  EXPECT_EQ(match_url, autocomplete_match.destination_url);
  EXPECT_EQ(expected_contents, autocomplete_match.contents);
  EXPECT_TRUE(std::equal(expected_contents_class.begin(),
                         expected_contents_class.end(),
                         autocomplete_match.contents_class.begin()));
  EXPECT_EQ(match_title, autocomplete_match.description);
  EXPECT_TRUE(std::equal(expected_description_class.begin(),
                         expected_description_class.end(),
                         autocomplete_match.description_class.begin()));
  EXPECT_EQ(expected_contents, autocomplete_match.fill_into_edit);
  EXPECT_TRUE(autocomplete_match.allowed_to_be_default_match);
  EXPECT_EQ(expected_inline_autocompletion,
            autocomplete_match.inline_autocompletion);
}

TEST(TitledUrlMatchUtilsTest, EmptyInlineAutocompletion) {
  // The search term matches the title but not the URL. Since there is no URL
  // match, the inline autocompletion string will be empty.
  base::string16 input_text(base::ASCIIToUTF16("goo"));
  base::string16 match_title(base::ASCIIToUTF16("Email by Google"));
  GURL match_url("http://www.gmail.com/");
  AutocompleteMatchType::Type type = AutocompleteMatchType::BOOKMARK_TITLE;
  int relevance = 123;

  MockTitledUrlNode node(match_title, match_url);
  bookmarks::TitledUrlMatch titled_url_match;
  titled_url_match.node = &node;
  titled_url_match.title_match_positions = {{9, 12}};
  titled_url_match.url_match_positions = {};

  scoped_refptr<MockAutocompleteProvider> provider =
      new MockAutocompleteProvider(AutocompleteProvider::Type::TYPE_BOOKMARK);
  TestSchemeClassifier classifier;
  AutocompleteInput input(input_text, metrics::OmniboxEventProto::NTP,
                          classifier);
  const base::string16 fixed_up_input(input_text);

  AutocompleteMatch autocomplete_match = TitledUrlMatchToAutocompleteMatch(
      titled_url_match, type, relevance, provider.get(), classifier, input,
      fixed_up_input);

  ACMatchClassifications expected_contents_class = {
      {0, ACMatchClassification::URL},
  };
  ACMatchClassifications expected_description_class = {
      {0, ACMatchClassification::NONE},
      {9, ACMatchClassification::MATCH},
      {12, ACMatchClassification::NONE},
  };

  // Because there is no match on the URL scheme, we should be able to trim
  // the HTTP scheme off.
  base::string16 expected_contents(base::ASCIIToUTF16("www.gmail.com"));

  EXPECT_EQ(provider.get(), autocomplete_match.provider);
  EXPECT_EQ(type, autocomplete_match.type);
  EXPECT_EQ(relevance, autocomplete_match.relevance);
  EXPECT_EQ(match_url, autocomplete_match.destination_url);
  EXPECT_EQ(expected_contents, autocomplete_match.contents);
  EXPECT_TRUE(std::equal(expected_contents_class.begin(),
                         expected_contents_class.end(),
                         autocomplete_match.contents_class.begin()));
  EXPECT_EQ(match_title, autocomplete_match.description);
  EXPECT_TRUE(std::equal(expected_description_class.begin(),
                         expected_description_class.end(),
                         autocomplete_match.description_class.begin()));
  EXPECT_EQ(expected_contents, autocomplete_match.fill_into_edit);
  EXPECT_FALSE(autocomplete_match.allowed_to_be_default_match);
  EXPECT_TRUE(autocomplete_match.inline_autocompletion.empty());
}

TEST(TitledUrlMatchUtilsTest, CorrectTitleAndMatchPositions) {
  bookmarks::TitledUrlMatch::MatchPositions match_positions = {{2, 6},
                                                               {10, 15}};
  base::string16 title = base::ASCIIToUTF16("  Leading whitespace");
  bookmarks::TitledUrlMatch::MatchPositions expected_match_positions = {
      {0, 4}, {8, 13}};
  base::string16 expected_title = base::ASCIIToUTF16("Leading whitespace");
  CorrectTitleAndMatchPositions(&title, &match_positions);
  EXPECT_EQ(expected_title, title);
  EXPECT_TRUE(std::equal(match_positions.begin(), match_positions.end(),
                         expected_match_positions.begin()));
}
