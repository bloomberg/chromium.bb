// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/intent_helper/local_activity_resolver.h"

#include <string>
#include <utility>

#include "components/arc/intent_helper/intent_filter.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace arc {

namespace {

IntentFilter GetIntentFilter(const std::string& host) {
  std::vector<IntentFilter::AuthorityEntry> authorities;
  authorities.emplace_back(host, -1);

  return IntentFilter(std::move(authorities),
                      std::vector<IntentFilter::PatternMatcher>());
}

}  // namespace

// Tests that ShouldChromeHandleUrl returns true by default.
TEST(LocalActivityResolverTest, TestDefault) {
  scoped_refptr<LocalActivityResolver> resolver(new LocalActivityResolver());
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("file:///etc/password")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("chrome://help")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("about://chrome")));
}

// Tests that ShouldChromeHandleUrl returns false when there's a match.
TEST(LocalActivityResolverTest, TestSingleFilter) {
  scoped_refptr<LocalActivityResolver> resolver(new LocalActivityResolver());

  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  resolver->UpdateIntentFilters(std::move(array));

  EXPECT_FALSE(resolver->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_FALSE(resolver->ShouldChromeHandleUrl(GURL("https://www.google.com")));

  EXPECT_TRUE(
      resolver->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
}

// Tests the same with multiple filters.
TEST(LocalActivityResolverTest, TestMultipleFilters) {
  scoped_refptr<LocalActivityResolver> resolver(new LocalActivityResolver());

  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  array.emplace_back(GetIntentFilter("www.google.co.uk"));
  array.emplace_back(GetIntentFilter("dev.chromium.org"));
  resolver->UpdateIntentFilters(std::move(array));

  EXPECT_FALSE(resolver->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_FALSE(resolver->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("http://www.google.co.uk")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("http://dev.chromium.org")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("https://dev.chromium.org")));

  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("http://www.android.com")));
}

// Tests that ShouldChromeHandleUrl returns true for non http(s) URLs.
TEST(LocalActivityResolverTest, TestNonHttp) {
  scoped_refptr<LocalActivityResolver> resolver(new LocalActivityResolver());

  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  resolver->UpdateIntentFilters(std::move(array));

  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("chrome://www.google.com")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("custom://www.google.com")));
}

// Tests that ShouldChromeHandleUrl discards the previous filters when
// UpdateIntentFilters is called with new ones.
TEST(LocalActivityResolverTest, TestMultipleUpdate) {
  scoped_refptr<LocalActivityResolver> resolver(new LocalActivityResolver());

  std::vector<IntentFilter> array;
  array.emplace_back(GetIntentFilter("www.google.com"));
  array.emplace_back(GetIntentFilter("dev.chromium.org"));
  resolver->UpdateIntentFilters(std::move(array));

  std::vector<IntentFilter> array2;
  array2.emplace_back(GetIntentFilter("www.google.co.uk"));
  array2.emplace_back(GetIntentFilter("dev.chromium.org"));
  array2.emplace_back(GetIntentFilter("www.android.com"));
  resolver->UpdateIntentFilters(std::move(array2));

  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("http://www.google.com")));
  EXPECT_TRUE(resolver->ShouldChromeHandleUrl(GURL("https://www.google.com")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("http://www.google.co.uk")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("https://www.google.co.uk")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("http://dev.chromium.org")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("https://dev.chromium.org")));
  EXPECT_FALSE(resolver->ShouldChromeHandleUrl(GURL("http://www.android.com")));
  EXPECT_FALSE(
      resolver->ShouldChromeHandleUrl(GURL("https://www.android.com")));
}

}  // namespace arc
