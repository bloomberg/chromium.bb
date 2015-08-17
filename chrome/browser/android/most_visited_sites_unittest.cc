// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/android/most_visited_sites.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct TitleURL {
  TitleURL(const std::string& title, const std::string& url)
      : title(base::UTF8ToUTF16(title)), url(url) {}

  base::string16 title;
  std::string url;
};

std::vector<base::string16> GetTitles(const std::vector<TitleURL>& data) {
  std::vector<base::string16> titles;
  for (const TitleURL& item : data)
    titles.push_back(item.title);
  return titles;
}

std::vector<std::string> GetURLs(const std::vector<TitleURL>& data) {
  std::vector<std::string> urls;
  for (const TitleURL& item : data)
    urls.push_back(item.url);
  return urls;
}

static const int kNumSites = 4;

}  // namespace

class MostVisitedSitesTest : public testing::Test {
 protected:
  void Check(const std::vector<TitleURL>& popular,
             const std::vector<TitleURL>& personal,
             const std::vector<TitleURL>& expected) {
    std::vector<base::string16> titles(GetTitles(personal));
    std::vector<std::string> urls(GetURLs(personal));

    std::vector<base::string16> popular_titles(GetTitles(popular));
    std::vector<std::string> popular_urls(GetURLs(popular));

    MostVisitedSites::AddPopularSitesImpl(
        kNumSites, &titles, &urls, popular_titles, popular_urls);

    EXPECT_EQ(GetTitles(expected), titles);
    EXPECT_EQ(GetURLs(expected), urls);
  }
};

TEST_F(MostVisitedSitesTest, PopularSitesAppend) {
  TitleURL popular[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
  };
  TitleURL personal[] = {
    TitleURL("Site 3", "https://www.site3.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
  };
  // Popular suggestions should keep their positions, with personal suggestions
  // appended at the end.
  TitleURL expected[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
  };

  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularSitesOverflow) {
  TitleURL popular[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
  };
  TitleURL personal[] = {
    TitleURL("Site 4", "https://www.site4.com/"),
    TitleURL("Site 5", "https://www.site5.com/"),
  };
  // When there are more total suggestions than slots, the personal suggestions
  // should win, with the remaining popular suggestions still retaining their
  // positions.
  TitleURL expected[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
    TitleURL("Site 5", "https://www.site5.com/"),
  };

  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularSitesOverwrite) {
  TitleURL popular[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
  };
  TitleURL personal[] = {
    TitleURL("Site 2 subpage", "https://www.site2.com/path"),
  };
  // When a personal suggestions matches the host of a popular one, it should
  // overwrite that suggestion (in its original position).
  TitleURL expected[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2 subpage", "https://www.site2.com/path"),
    TitleURL("Site 3", "https://www.site3.com/"),
  };

  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularSitesOrdering) {
  TitleURL popular[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
  };
  TitleURL personal[] = {
    TitleURL("Site 3", "https://www.site3.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
  };
  // The order of the popular sites should win (since presumably they were
  // there first).
  TitleURL expected[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
    TitleURL("Site 4", "https://www.site4.com/"),
  };

  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}

TEST_F(MostVisitedSitesTest, PopularSitesComplex) {
  TitleURL popular[] = {
    TitleURL("Site 1", "https://www.site1.com/"),
    TitleURL("Site 2", "https://www.site2.com/"),
    TitleURL("Site 3", "https://www.site3.com/"),
  };
  TitleURL personal[] = {
    TitleURL("Site 3 subpage", "https://www.site3.com/path"),
    TitleURL("Site 1 subpage", "https://www.site1.com/path"),
    TitleURL("Site 5", "https://www.site5.com/"),
    TitleURL("Site 6", "https://www.site6.com/"),
  };
  // Combination of behaviors: Personal suggestions replace matching popular
  // ones while keeping the position of the popular suggestion. Remaining
  // personal suggestions evict popular ones and retain their relative order.
  TitleURL expected[] = {
    TitleURL("Site 1 subpage", "https://www.site1.com/path"),
    TitleURL("Site 5", "https://www.site5.com/"),
    TitleURL("Site 3 subpage", "https://www.site3.com/path"),
    TitleURL("Site 6", "https://www.site6.com/"),
  };

  Check(std::vector<TitleURL>(popular, popular + arraysize(popular)),
        std::vector<TitleURL>(personal, personal + arraysize(personal)),
        std::vector<TitleURL>(expected, expected + arraysize(expected)));
}
