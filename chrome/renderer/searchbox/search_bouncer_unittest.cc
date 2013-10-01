// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/search_bouncer.h"

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "chrome/common/render_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class SearchBouncerTest : public testing::Test {
 public:
  virtual void SetUp() {
    bouncer_.reset(new SearchBouncer);
    std::vector<GURL> search_urls;
    search_urls.push_back(GURL("http://example.com/search"));
    search_urls.push_back(GURL("http://example.com/search2"));
    bouncer_->OnSetSearchURLs(search_urls, GURL("http://example.com/newtab"));
  }

  scoped_ptr<SearchBouncer> bouncer_;
};

TEST_F(SearchBouncerTest, ShouldFork) {
  EXPECT_FALSE(bouncer_->ShouldFork(GURL()));
  EXPECT_FALSE(bouncer_->ShouldFork(GURL("http://notsearch.example.com")));
  EXPECT_TRUE(bouncer_->ShouldFork(GURL("http://example.com/search?q=foo")));
  EXPECT_TRUE(bouncer_->ShouldFork(GURL("http://example.com/search2#q=foo")));
  EXPECT_TRUE(bouncer_->ShouldFork(GURL("http://example.com/newtab")));
}

TEST_F(SearchBouncerTest, IsNewTabPage) {
  EXPECT_FALSE(bouncer_->IsNewTabPage(GURL("http://example.com/foo")));
  EXPECT_TRUE(bouncer_->IsNewTabPage(GURL("http://example.com/newtab")));
}

TEST_F(SearchBouncerTest, SetSearchURLs) {
  std::vector<GURL> search_urls;
  search_urls.push_back(GURL("http://other.example.com/search"));
  const GURL new_tab_page_url(GURL("http://other.example.com/newtab"));
  EXPECT_TRUE(bouncer_->OnControlMessageReceived(
      ChromeViewMsg_SetSearchURLs(search_urls, new_tab_page_url)));
  EXPECT_EQ(search_urls, bouncer_->search_urls_);
  EXPECT_EQ(new_tab_page_url, bouncer_->new_tab_page_url_);
}
