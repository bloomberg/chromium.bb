// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_dispatcher.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace prerender {

class PrerenderDispatcherTest : public testing::Test {
 public:
  PrerenderDispatcherTest() {}

  bool is_prerender_url(const GURL& url) const {
    return prerender_dispatcher_.IsPrerenderURL(url);
  }

  const PrerenderDispatcher::PrerenderMap& urls() const {
    return prerender_dispatcher_.prerender_urls_;
  }

  void AddURL(const GURL& url) { prerender_dispatcher_.OnAddPrerenderURL(url); }
  void RemoveURL(const GURL& url) {
    prerender_dispatcher_.OnRemovePrerenderURL(url);
  }

  int GetCountForURL(const GURL& url) const {
    PrerenderDispatcher::PrerenderMap::const_iterator entry = urls().find(url);
    if (entry == urls().end())
      return 0;
    EXPECT_GT(entry->second, 0);
    return entry->second;
  }

 private:
  PrerenderDispatcher prerender_dispatcher_;
  DISALLOW_COPY_AND_ASSIGN(PrerenderDispatcherTest);
};

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherEmpty) {
  EXPECT_EQ(0U, urls().size());
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherSingleAdd) {
  GURL foo_url = GURL("http://foo.com");
  EXPECT_FALSE(is_prerender_url(foo_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_EQ(1, GetCountForURL(foo_url));
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherMultipleAdd) {
  GURL foo_url = GURL("http://foo.com");
  GURL bar_url = GURL("http://bar.com");

  EXPECT_FALSE(is_prerender_url(foo_url));
  EXPECT_FALSE(is_prerender_url(bar_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_FALSE(is_prerender_url(bar_url));

  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_FALSE(is_prerender_url(bar_url));
  EXPECT_EQ(2, GetCountForURL(foo_url));

  AddURL(bar_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_TRUE(is_prerender_url(bar_url));
  EXPECT_EQ(2, GetCountForURL(foo_url));
  EXPECT_EQ(1, GetCountForURL(bar_url));
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherSingleRemove) {
  GURL foo_url = GURL("http://foo.com");
  EXPECT_FALSE(is_prerender_url(foo_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  RemoveURL(foo_url);
  EXPECT_FALSE(is_prerender_url(foo_url));
  EXPECT_EQ(0, GetCountForURL(foo_url));
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherMultipleRemove) {
  GURL foo_url = GURL("http://foo.com");
  EXPECT_FALSE(is_prerender_url(foo_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_EQ(2, GetCountForURL(foo_url));

  RemoveURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  EXPECT_EQ(1, GetCountForURL(foo_url));

  RemoveURL(foo_url);
  EXPECT_FALSE(is_prerender_url(foo_url));
  EXPECT_EQ(0, GetCountForURL(foo_url));
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherRemoveWithoutAdd) {
  GURL foo_url = GURL("http://foo.com");
  EXPECT_FALSE(is_prerender_url(foo_url));
  RemoveURL(foo_url);
  EXPECT_FALSE(is_prerender_url(foo_url));
  EXPECT_EQ(0, GetCountForURL(foo_url));
}

TEST_F(PrerenderDispatcherTest, PrerenderDispatcherRemoveTooMany) {
  GURL foo_url = GURL("http://foo.com");
  EXPECT_FALSE(is_prerender_url(foo_url));
  AddURL(foo_url);
  EXPECT_TRUE(is_prerender_url(foo_url));
  RemoveURL(foo_url);
  EXPECT_FALSE(is_prerender_url(foo_url));
  RemoveURL(foo_url);
  EXPECT_FALSE(is_prerender_url(foo_url));
  EXPECT_EQ(0, GetCountForURL(foo_url));
}

}  // end namespace prerender
