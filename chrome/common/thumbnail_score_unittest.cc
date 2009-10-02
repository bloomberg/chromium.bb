// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/thumbnail_score.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests that the different types of thumbnails are compared properly.
TEST(ThumbnailScoreTest, ShouldReplaceThumbnailWithType) {
  base::Time now = base::Time::Now();

  ThumbnailScore nothing_good(0.5, false, false, now);
  ThumbnailScore not_at_top(0.5, false, true, now);
  ThumbnailScore bad_clipping(0.5, true, false, now);
  ThumbnailScore life_is_awesome(0.5, true, true, now);

  EXPECT_TRUE(ShouldReplaceThumbnailWith(nothing_good, not_at_top));
  EXPECT_TRUE(ShouldReplaceThumbnailWith(nothing_good, bad_clipping));
  EXPECT_TRUE(ShouldReplaceThumbnailWith(nothing_good, life_is_awesome));
  EXPECT_TRUE(ShouldReplaceThumbnailWith(not_at_top, bad_clipping));
  EXPECT_TRUE(ShouldReplaceThumbnailWith(not_at_top, life_is_awesome));
  EXPECT_TRUE(ShouldReplaceThumbnailWith(bad_clipping, life_is_awesome));
}

// Tests that we'll replace old thumbnails will crappier but newer ones.
TEST(ThumbnailScoreTest, ShouldReplaceThumbnailWithTime) {
  // Use a really long time for the difference so we aren't sensitive to the
  // degrading schedule.
  base::Time now = base::Time::Now();
  base::Time last_year = now - base::TimeDelta::FromDays(365);

  ThumbnailScore oldie_but_goodie(0.1, true, true, last_year);
  ThumbnailScore newie_but_crappie(0.9, true, true, now);

  EXPECT_TRUE(ShouldReplaceThumbnailWith(oldie_but_goodie, newie_but_crappie));
}

// Having many redirects should age the thumbnail.
TEST(ThumbnailScoreTest, RedirectCount) {
  base::Time now = base::Time::Now();

  ThumbnailScore no_redirects(0.5, true, true, now);
  no_redirects.redirect_hops_from_dest = 0;
  ThumbnailScore some_redirects(0.5, true, true, now);
  some_redirects.redirect_hops_from_dest = 1;

  EXPECT_TRUE(ShouldReplaceThumbnailWith(some_redirects, no_redirects));

  // This one has a lot of redirects but a better score. It should still be
  // rejected.
  ThumbnailScore lotsa_redirects(0.4, true, true, now);
  lotsa_redirects.redirect_hops_from_dest = 4;
  EXPECT_FALSE(ShouldReplaceThumbnailWith(no_redirects, lotsa_redirects));
}
