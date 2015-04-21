// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_util.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace prerender {

// Ensure that extracting a urlencoded URL in the url= query string component
// works.
TEST(PrerenderUtilTest, ExtractURLInQueryStringTest) {
  GURL result;
  EXPECT_TRUE(MaybeGetQueryStringBasedAliasURL(
      GURL(
          "http://www.google.com/url?sa=t&source=web&cd=1&ved=0CBcQFjAA&url=h"
          "ttp%3A%2F%2Fwww.abercrombie.com%2Fwebapp%2Fwcs%2Fstores%2Fservlet%"
          "2FStoreLocator%3FcatalogId%3D%26storeId%3D10051%26langId%3D-1&rct="
          "j&q=allinurl%3A%26&ei=KLyUTYGSEdTWiAKUmLCdCQ&usg=AFQjCNF8nJ2MpBFfr"
          "1ijO39_f22bcKyccw&sig2=2ymyGpO0unJwU1d4kdCUjQ"),
      &result));
  ASSERT_EQ(GURL(
                "http://www.abercrombie.com/webapp/wcs/stores/servlet/StoreLo"
                "cator?catalogId=&storeId=10051&langId=-1").spec(),
            result.spec());
  EXPECT_FALSE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/url?sadf=test&blah=blahblahblah"), &result));
  EXPECT_FALSE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=INVALIDurlsAREsoMUCHfun.com"), &result));
  EXPECT_TRUE(MaybeGetQueryStringBasedAliasURL(
      GURL("http://www.google.com/?url=http://validURLSareGREAT.com"),
      &result));
  ASSERT_EQ(GURL("http://validURLSareGREAT.com").spec(), result.spec());
}

// Ensure that we detect Google search result URLs correctly.
TEST(PrerenderUtilTest, DetectGoogleSearchREsultURLTest) {
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/#asdf")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/?a=b")));
  EXPECT_TRUE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/search?q=hi")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/search")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.com/webhp")));
  EXPECT_TRUE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/webhp?a=b#123")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://www.google.com/imgres")));
  EXPECT_FALSE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/imgres?q=hi")));
  EXPECT_FALSE(IsGoogleSearchResultURL(
      GURL("http://www.google.com/imgres?q=hi#123")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://google.com/search")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://WWW.GooGLE.CoM/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://WWW.GooGLE.CoM/SeArcH")));
  EXPECT_TRUE(IsGoogleSearchResultURL(GURL("http://www.google.co.uk/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://google.co.uk/search")));
  EXPECT_FALSE(IsGoogleSearchResultURL(GURL("http://www.chromium.org/search")));
}

}  // namespace prerender
