// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings_helper.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ContentSettingsHelperTest, OriginToWString) {
  // Urls with "http":
  const GURL kUrl0("http://www.foo.com/bar");
  const GURL kUrl1("http://foo.com/bar");

  const GURL kUrl2("http://www.foo.com:81/bar");
  const GURL kUrl3("http://foo.com:81/bar");

  // Urls with "https":
  const GURL kUrl4("https://www.foo.com/bar");
  const GURL kUrl5("https://foo.com/bar");

  const GURL kUrl6("https://www.foo.com:81/bar");
  const GURL kUrl7("https://foo.com:81/bar");

  // Now check the first group of urls with just "http":
  EXPECT_EQ(L"www.foo.com", content_settings_helper::OriginToWString(kUrl0));
  EXPECT_EQ(L"foo.com", content_settings_helper::OriginToWString(kUrl1));

  EXPECT_EQ(L"www.foo.com:81", content_settings_helper::OriginToWString(kUrl2));
  EXPECT_EQ(L"foo.com:81", content_settings_helper::OriginToWString(kUrl3));

  // Now check the second group of urls with "https":
  EXPECT_EQ(L"https://www.foo.com",
            content_settings_helper::OriginToWString(kUrl4));
  EXPECT_EQ(L"https://foo.com",
            content_settings_helper::OriginToWString(kUrl5));

  EXPECT_EQ(L"https://www.foo.com:81",
            content_settings_helper::OriginToWString(kUrl6));
  EXPECT_EQ(L"https://foo.com:81",
            content_settings_helper::OriginToWString(kUrl7));
}
