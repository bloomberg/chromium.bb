// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/content_settings_helper.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

TEST(ContentSettingsHelperTest, OriginToString16) {
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
  EXPECT_EQ(base::ASCIIToUTF16("www.foo.com"),
            content_settings_helper::OriginToString16(kUrl0));
  EXPECT_EQ(base::ASCIIToUTF16("foo.com"),
            content_settings_helper::OriginToString16(kUrl1));

  EXPECT_EQ(base::ASCIIToUTF16("www.foo.com:81"),
            content_settings_helper::OriginToString16(kUrl2));
  EXPECT_EQ(base::ASCIIToUTF16("foo.com:81"),
            content_settings_helper::OriginToString16(kUrl3));

  // Now check the second group of urls with "https":
  EXPECT_EQ(base::ASCIIToUTF16("https://www.foo.com"),
            content_settings_helper::OriginToString16(kUrl4));
  EXPECT_EQ(base::ASCIIToUTF16("https://foo.com"),
            content_settings_helper::OriginToString16(kUrl5));

  EXPECT_EQ(base::ASCIIToUTF16("https://www.foo.com:81"),
            content_settings_helper::OriginToString16(kUrl6));
  EXPECT_EQ(base::ASCIIToUTF16("https://foo.com:81"),
            content_settings_helper::OriginToString16(kUrl7));
}
