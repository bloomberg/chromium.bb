// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/history/top_sites.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace history {

class TopSitesTest : public testing::Test {
 public:
  TopSitesTest() : top_sites_(new TopSites) {
  }
  ~TopSitesTest() {
  }

  TopSites& top_sites() { return *top_sites_; }

  virtual void SetUp() {
  }

  virtual void TearDown() {
  }

  // Wrappers that allow private TopSites functions to be called from the
  // individual tests without making them all be friends.
  GURL GetCanonicalURL(const GURL& url) const {
    AutoLock lock(top_sites_->lock_);  // The function asserts it's in the lock.
    return top_sites_->GetCanonicalURL(url);
  }

  void StoreMostVisited(std::vector<MostVisitedURL>* urls) {
    AutoLock lock(top_sites_->lock_);  // The function asserts it's in the lock.
    top_sites_->StoreMostVisited(urls);
  }

  static void DiffMostVisited(const std::vector<MostVisitedURL>& old_list,
                              const std::vector<MostVisitedURL>& new_list,
                              std::vector<size_t>* added_urls,
                              std::vector<size_t>* deleted_urls,
                              std::vector<size_t>* moved_urls)  {
    TopSites::DiffMostVisited(old_list, new_list,
                              added_urls, deleted_urls, moved_urls);
  }

 private:
  scoped_refptr<TopSites> top_sites_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesTest);
};

// Helper function for appending a URL to a vector of "most visited" URLs,
// using the default values for everything but the URL.
static void AppendMostVisitedURL(std::vector<MostVisitedURL>* list,
                                 const GURL& url) {
  MostVisitedURL mv;
  mv.url = url;
  mv.redirects.push_back(url);
  list->push_back(mv);
}

// Same as AppendMostVisitedURL except that it adds a redirect from the first
// URL to the second.
static void AppendMostVisitedURLWithRedirect(
    std::vector<MostVisitedURL>* list,
    const GURL& redirect_source, const GURL& redirect_dest) {
  MostVisitedURL mv;
  mv.url = redirect_dest;
  mv.redirects.push_back(redirect_source);
  mv.redirects.push_back(redirect_dest);
  list->push_back(mv);
}

TEST_F(TopSitesTest, GetCanonicalURL) {
  // Have two chains:
  //   google.com -> www.google.com
  //   news.google.com (no redirects)
  GURL news("http://news.google.com/");
  GURL source("http://google.com/");
  GURL dest("http://www.google.com/");

  std::vector<MostVisitedURL> most_visited;
  AppendMostVisitedURLWithRedirect(&most_visited, source, dest);
  AppendMostVisitedURL(&most_visited, news);
  StoreMostVisited(&most_visited);

  // Random URLs not in the database shouldn't be reported as being in there.
  GURL result = GetCanonicalURL(GURL("http://fark.com/"));
  EXPECT_TRUE(result.is_empty());

  // Easy case, there are no redirects and the exact URL is stored.
  result = GetCanonicalURL(news);
  EXPECT_EQ(news, result);

  // The URL in question is the source URL in a redirect list.
  result = GetCanonicalURL(source);
  EXPECT_EQ(dest, result);

  // The URL in question is the destination of a redirect.
  result = GetCanonicalURL(dest);
  EXPECT_EQ(dest, result);
}

TEST_F(TopSitesTest, DiffMostVisited) {
  GURL stays_the_same("http://staysthesame/");
  GURL gets_added_1("http://getsadded1/");
  GURL gets_added_2("http://getsadded2/");
  GURL gets_deleted_1("http://getsdeleted2/");
  GURL gets_moved_1("http://getsmoved1/");

  std::vector<MostVisitedURL> old_list;
  AppendMostVisitedURL(&old_list, stays_the_same);  // 0  (unchanged)
  AppendMostVisitedURL(&old_list, gets_deleted_1);  // 1  (deleted)
  AppendMostVisitedURL(&old_list, gets_moved_1);    // 2  (moved to 3)

  std::vector<MostVisitedURL> new_list;
  AppendMostVisitedURL(&new_list, stays_the_same);  // 0  (unchanged)
  AppendMostVisitedURL(&new_list, gets_added_1);    // 1  (added)
  AppendMostVisitedURL(&new_list, gets_added_2);    // 2  (added)
  AppendMostVisitedURL(&new_list, gets_moved_1);    // 3  (moved from 2)

  std::vector<size_t> added;
  std::vector<size_t> deleted;
  std::vector<size_t> moved;
  DiffMostVisited(old_list, new_list, &added, &deleted, &moved);

  ASSERT_EQ(2u, added.size());
  ASSERT_EQ(1u, deleted.size());
  ASSERT_EQ(1u, moved.size());

  // There should be 2 URLs added, we don't assume what order they're in inside
  // the result vector.
  EXPECT_TRUE(added[0] == 1 || added[1] == 1);
  EXPECT_TRUE(added[0] == 2 || added[1] == 2);

  EXPECT_EQ(1u, deleted[0]);
  EXPECT_EQ(3u, moved[0]);
}

TEST_F(TopSitesTest, SetPageThumbnail) {
  GURL url1a("http://google.com/");
  GURL url1b("http://www.google.com/");
  GURL url2("http://images.google.com/");
  GURL nonexistant_url("http://news.google.com/");

  std::vector<MostVisitedURL> list;
  AppendMostVisitedURL(&list, url2);

  MostVisitedURL mv;
  mv.url = url1b;
  mv.redirects.push_back(url1a);
  mv.redirects.push_back(url1b);
  list.push_back(mv);

  // Save our most visited data containing that one site.
  StoreMostVisited(&list);

  // Create a dummy thumbnail.
  SkBitmap thumbnail;
  thumbnail.setConfig(SkBitmap::kARGB_8888_Config, 4, 4);
  thumbnail.allocPixels();

  base::Time now = base::Time::Now();
  ThumbnailScore low_score(1.0, true, true, now);
  ThumbnailScore medium_score(0.5, true, true, now);
  ThumbnailScore high_score(0.0, true, true, now);

  // Setting the thumbnail for nonexistant pages should fail.
  EXPECT_FALSE(top_sites().SetPageThumbnail(nonexistant_url,
                                            thumbnail, medium_score));

  // Setting the thumbnail for url2 should succeed, lower scores shouldn't
  // replace it, higher scores should.
  EXPECT_TRUE(top_sites().SetPageThumbnail(url2, thumbnail, medium_score));
  EXPECT_FALSE(top_sites().SetPageThumbnail(url2, thumbnail, low_score));
  EXPECT_TRUE(top_sites().SetPageThumbnail(url2, thumbnail, high_score));

  // Set on the redirect source should succeed. It should be replacable by
  // the same score on the redirect destination, which in turn should not
  // be replaced by the source again.
  EXPECT_TRUE(top_sites().SetPageThumbnail(url1a, thumbnail, medium_score));
  EXPECT_TRUE(top_sites().SetPageThumbnail(url1b, thumbnail, medium_score));
  EXPECT_FALSE(top_sites().SetPageThumbnail(url1a, thumbnail, medium_score));
}

}  // namespace history
