// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/most_visited_sites.h"

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TitleURL {
  TitleURL(const std::string& title, const std::string& url)
      : title(base::UTF8ToUTF16(title)), url(url) {}
  TitleURL(const base::string16& title, const std::string& url)
      : title(title), url(url) {}

  base::string16 title;
  std::string url;

  bool operator==(const TitleURL& other) const {
    return title == other.title && url == other.url;
  }
};

static const size_t kNumSites = 4;

}  // namespace

// This a test for MostVisitedSites::MergeSuggestions(...) method, and thus has
// the same scope as the method itself. This includes:
// + Merge popular suggestions with personal suggestions.
// + Order the suggestions correctly based on the previous ordering.
// More importantly things out of the scope of testing presently:
// - Removing blacklisted suggestions.
// - Storing the current suggestion ordering.
// - Retrieving the previous ordering.
// - Correct Host extraction from the URL.
// - Ensuring personal suggestions are not duplicated in popular suggestions.
class MostVisitedSitesTest : public testing::Test {
 protected:
  void Check(const std::vector<TitleURL>& popular_sites,
             const std::vector<TitleURL>& whitelist_entry_points,
             const std::vector<TitleURL>& personal_sites,
             const std::vector<std::string>& old_sites_url,
             const std::vector<bool>& old_sites_is_personal,
             const std::vector<bool>& expected_sites_is_personal,
             const std::vector<TitleURL>& expected_sites) {
    MostVisitedSites::SuggestionsPtrVector personal_suggestions;
    for (const TitleURL& site : personal_sites)
      personal_suggestions.push_back(MakeSuggestionFrom(site, true, false));
    MostVisitedSites::SuggestionsPtrVector whitelist_suggestions;
    for (const TitleURL& site : whitelist_entry_points)
      whitelist_suggestions.push_back(MakeSuggestionFrom(site, false, true));
    MostVisitedSites::SuggestionsPtrVector popular_suggestions;
    for (const TitleURL& site : popular_sites)
      popular_suggestions.push_back(MakeSuggestionFrom(site, false, false));
    MostVisitedSites::SuggestionsPtrVector result_suggestions =
        MostVisitedSites::MergeSuggestions(
            &personal_suggestions, &whitelist_suggestions, &popular_suggestions,
            old_sites_url, old_sites_is_personal);
    std::vector<TitleURL> result_sites;
    std::vector<bool> result_is_personal;
    result_sites.reserve(result_suggestions.size());
    result_is_personal.reserve(result_suggestions.size());
    for (const auto& suggestion : result_suggestions) {
      result_sites.push_back(
          TitleURL(suggestion->title, suggestion->url.spec()));
      result_is_personal.push_back(suggestion->source !=
                                   MostVisitedSites::POPULAR);
    }
    EXPECT_EQ(result_is_personal, expected_sites_is_personal);
    EXPECT_EQ(result_sites, expected_sites);
  }
  static std::unique_ptr<MostVisitedSites::Suggestion> MakeSuggestionFrom(
      const TitleURL& title_url,
      bool is_personal,
      bool whitelist) {
    std::unique_ptr<MostVisitedSites::Suggestion> suggestion =
        base::WrapUnique(new MostVisitedSites::Suggestion());
    suggestion->title = title_url.title;
    suggestion->url = GURL(title_url.url);
    suggestion->source = whitelist ? MostVisitedSites::WHITELIST
                                   : (is_personal ? MostVisitedSites::TOP_SITES
                                                  : MostVisitedSites::POPULAR);
    return suggestion;
  }
};

TEST_F(MostVisitedSitesTest, PersonalSitesDefaultOrder) {
  TitleURL personal[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  std::vector<TitleURL> personal_sites(personal,
                                       personal + arraysize(personal));
  std::vector<std::string> old_sites_url;
  std::vector<bool> old_sites_source;
  // Without any previous ordering or popular suggestions, the result after
  // merge should be the personal suggestions themselves.
  std::vector<bool> expected_sites_source(kNumSites, true /*personal source*/);
  Check(std::vector<TitleURL>(), std::vector<TitleURL>(), personal_sites,
        old_sites_url, old_sites_source, expected_sites_source, personal_sites);
}

TEST_F(MostVisitedSitesTest, PersonalSitesDefinedOrder) {
  TitleURL personal[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  std::string old[] = {
      "https://www.site4.com/", "https://www.site2.com/",
  };
  std::vector<bool> old_sites_source(arraysize(old), true /*personal source*/);
  TitleURL expected[] = {
      TitleURL("Site 4", "https://www.site4.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
  };
  std::vector<bool> expected_sites_source(kNumSites, true /*personal source*/);
  Check(std::vector<TitleURL>(), std::vector<TitleURL>(),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<std::string>(old, old + arraysize(old)), old_sites_source,
        expected_sites_source,
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularSitesDefaultOrder) {
  TitleURL popular[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  std::vector<TitleURL> popular_sites(popular, popular + arraysize(popular));
  std::vector<std::string> old_sites_url;
  std::vector<bool> old_sites_source;
  // Without any previous ordering or personal suggestions, the result after
  // merge should be the popular suggestions themselves.
  std::vector<bool> expected_sites_source(kNumSites, false /*popular source*/);
  Check(popular_sites, std::vector<TitleURL>(), std::vector<TitleURL>(),
        old_sites_url, old_sites_source, expected_sites_source, popular_sites);
}

TEST_F(MostVisitedSitesTest, PopularSitesDefinedOrder) {
  TitleURL popular[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  std::string old[] = {
      "https://www.site4.com/", "https://www.site2.com/",
  };
  std::vector<bool> old_sites_source(arraysize(old), false /*popular source*/);
  TitleURL expected[] = {
      TitleURL("Site 4", "https://www.site4.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
  };
  std::vector<bool> expected_sites_source(kNumSites, false /*popular source*/);
  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(), std::vector<TitleURL>(),
        std::vector<std::string>(old, old + arraysize(old)), old_sites_source,
        expected_sites_source,
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularAndPersonalDefaultOrder) {
  TitleURL popular[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
  };
  TitleURL personal[] = {
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Without an explicit ordering, personal suggestions precede popular
  // suggestions.
  TitleURL expected[] = {
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
  };
  bool expected_source_is_personal[] = {true, true, false, false};
  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<std::string>(), std::vector<bool>(),
        std::vector<bool>(expected_source_is_personal,
                          expected_source_is_personal +
                              arraysize(expected_source_is_personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularAndPersonalDefinedOrder) {
  TitleURL popular[] = {
      TitleURL("Site 1", "https://www.site1.com/"),
      TitleURL("Site 2", "https://www.site2.com/"),
  };
  TitleURL personal[] = {
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
  };
  std::string old[] = {
      "https://www.site2.com/", "https://www.unknownsite.com/",
      "https://www.site4.com/",
  };
  std::vector<bool> old_sites_source(arraysize(old), false /*popular source*/);
  // Keep the order constant for previous suggestions, else personal suggestions
  // precede popular suggestions.
  TitleURL expected[] = {
      TitleURL("Site 2", "https://www.site2.com/"),
      TitleURL("Site 3", "https://www.site3.com/"),
      TitleURL("Site 4", "https://www.site4.com/"),
      TitleURL("Site 1", "https://www.site1.com/"),
  };
  bool expected_source_is_personal[] = {false, true, true, false};
  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<std::string>(old, old + arraysize(old)), old_sites_source,
        std::vector<bool>(expected_source_is_personal,
                          expected_source_is_personal +
                              arraysize(expected_source_is_personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}
