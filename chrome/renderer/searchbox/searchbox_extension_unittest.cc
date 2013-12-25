// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace internal {

// Defined in searchbox_extension.cc
GURL ResolveURL(const GURL& current_url,
                const base::string16& possibly_relative_url);

TEST(SearchboxExtensionTest, ResolveURL) {
  EXPECT_EQ(GURL("http://www.google.com/"),
            ResolveURL(GURL(""), base::ASCIIToUTF16("http://www.google.com")));

  EXPECT_EQ(GURL("http://news.google.com/"),
            ResolveURL(GURL("http://www.google.com"),
                       base::ASCIIToUTF16("http://news.google.com")));

  EXPECT_EQ(GURL("http://www.google.com/hello?q=world"),
            ResolveURL(GURL("http://www.google.com/foo?a=b"),
                       base::ASCIIToUTF16("hello?q=world")));

  EXPECT_EQ(GURL("http://www.google.com:90/foo/hello?q=world"),
            ResolveURL(GURL("http://www.google.com:90/"),
                       base::ASCIIToUTF16("foo/hello?q=world")));
}

}  // namespace internal
