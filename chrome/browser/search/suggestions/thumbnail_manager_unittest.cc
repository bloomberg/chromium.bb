// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/browser/search/suggestions/proto/suggestions.pb.h"
#include "chrome/browser/search/suggestions/thumbnail_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTestUrl[] = "http://go.com/";
const char kTestThumbnailUrl[] = "http://thumb.com/anchor_download_test.png";

class ThumbnailManagerTest : public testing::Test {
 protected:
  content::TestBrowserThreadBundle thread_bundle_;
};

}  // namespace

namespace suggestions {

TEST_F(ThumbnailManagerTest, InitializeThumbnailMapTest) {
  SuggestionsProfile suggestions_profile;
  ChromeSuggestion* suggestion = suggestions_profile.add_suggestions();
  suggestion->set_url(kTestUrl);
  suggestion->set_thumbnail(kTestThumbnailUrl);

  TestingProfile profile;
  ThumbnailManager thumbnail_manager(&profile);
  thumbnail_manager.InitializeThumbnailMap(suggestions_profile);

  GURL output;
  EXPECT_TRUE(thumbnail_manager.GetThumbnailURL(GURL(kTestUrl), &output));
  EXPECT_EQ(GURL(kTestThumbnailUrl), output);

  EXPECT_FALSE(thumbnail_manager.GetThumbnailURL(GURL("http://b.com"),
                                                 &output));
}

}  // namespace suggestions
