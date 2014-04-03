// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_promo.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace signin {

TEST(SigninPromoTest, GetNextPageURLForPromoURL_ValidContinueURL) {
  GURL promo_url = GetPromoURLWithContinueURL(SOURCE_MENU,
                                              false /* auto_close */,
                                              false /* is_constrained */,
                                              GURL("https://www.example.com"));
  EXPECT_EQ(GURL("https://www.example.com"),
            GetNextPageURLForPromoURL(promo_url));
}

TEST(SigninPromoTest, GetNextPageURLForPromoURL_EmptyContinueURL) {
  GURL promo_url = GetPromoURLWithContinueURL(SOURCE_MENU,
                                              false /* auto_close */,
                                              false /* is_constrained */,
                                              GURL());
  EXPECT_TRUE(GetNextPageURLForPromoURL(promo_url).is_empty());
}

}  // namespace signin
