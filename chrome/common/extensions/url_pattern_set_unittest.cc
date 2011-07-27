// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

static void AddPattern(URLPatternSet* set, const std::string& pattern) {
  int schemes = URLPattern::SCHEME_ALL;
  set->AddPattern(URLPattern(schemes, pattern));
}

}
TEST(URLPatternSetTest, Empty) {
  URLPatternSet set;
  EXPECT_FALSE(set.MatchesURL(GURL("http://www.foo.com/bar")));
  EXPECT_FALSE(set.MatchesURL(GURL()));
  EXPECT_FALSE(set.MatchesURL(GURL("invalid")));
}

TEST(URLPatternSetTest, One) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");

  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/")));
  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/monkey")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.google.com/")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.microsoft.com/")));
}

TEST(URLPatternSetTest, Two) {
  URLPatternSet set;
  AddPattern(&set, "http://www.google.com/*");
  AddPattern(&set, "http://www.yahoo.com/*");

  EXPECT_TRUE(set.MatchesURL(GURL("http://www.google.com/monkey")));
  EXPECT_TRUE(set.MatchesURL(GURL("http://www.yahoo.com/monkey")));
  EXPECT_FALSE(set.MatchesURL(GURL("https://www.apple.com/monkey")));
}

TEST(URLPatternSetTest, OverlapsWith) {
  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");

  URLPatternSet set3;
  AddPattern(&set3, "http://www.google.com/q/*");
  AddPattern(&set3, "http://www.yahoo.com/b/*");

  EXPECT_FALSE(set1.OverlapsWith(set2));
  EXPECT_FALSE(set2.OverlapsWith(set1));

  EXPECT_TRUE(set1.OverlapsWith(set3));
  EXPECT_TRUE(set3.OverlapsWith(set1));
}

TEST(URLPatternSetTest, CreateDifference) {
  URLPatternSet expected;
  URLPatternSet set1;
  URLPatternSet set2;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  // Subtract an empty set.
  URLPatternSet result;
  URLPatternSet::CreateDifference(set1, set2, &result);
  EXPECT_EQ(set1, result);

  // Subtract a real set.
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.yahoo.com/b*");

  result.ClearPatterns();
  URLPatternSet::CreateDifference(set1, set2, &result);
  EXPECT_EQ(expected, result);
  EXPECT_FALSE(result.is_empty());
  EXPECT_TRUE(set1.Contains(result));
  EXPECT_FALSE(result.Contains(set2));
  EXPECT_FALSE(set2.Contains(result));

  URLPatternSet intersection;
  URLPatternSet::CreateIntersection(result, set2, &intersection);
  EXPECT_TRUE(intersection.is_empty());
}

TEST(URLPatternSetTest, CreateIntersection) {
  URLPatternSet empty_set;
  URLPatternSet expected;
  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  // Intersection with an empty set.
  URLPatternSet result;
  URLPatternSet::CreateIntersection(set1, empty_set, &result);
  EXPECT_EQ(expected, result);
  EXPECT_TRUE(result.is_empty());
  EXPECT_TRUE(empty_set.Contains(result));
  EXPECT_TRUE(result.Contains(empty_set));
  EXPECT_TRUE(set1.Contains(result));

  // Intersection with a real set.
  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.google.com/f*");

  result.ClearPatterns();
  URLPatternSet::CreateIntersection(set1, set2, &result);
  EXPECT_EQ(expected, result);
  EXPECT_FALSE(result.is_empty());
  EXPECT_TRUE(set1.Contains(result));
  EXPECT_TRUE(set2.Contains(result));
}

TEST(URLPatternSetTest, CreateUnion) {
  URLPatternSet empty_set;

  URLPatternSet set1;
  AddPattern(&set1, "http://www.google.com/f*");
  AddPattern(&set1, "http://www.yahoo.com/b*");

  URLPatternSet expected;
  AddPattern(&expected, "http://www.google.com/f*");
  AddPattern(&expected, "http://www.yahoo.com/b*");

  // Union with an empty set.
  URLPatternSet result;
  URLPatternSet::CreateUnion(set1, empty_set, &result);
  EXPECT_EQ(expected, result);

  // Union with a real set.
  URLPatternSet set2;
  AddPattern(&set2, "http://www.reddit.com/f*");
  AddPattern(&set2, "http://www.yahoo.com/z*");
  AddPattern(&set2, "http://www.google.com/f*");

  AddPattern(&expected, "http://www.reddit.com/f*");
  AddPattern(&expected, "http://www.yahoo.com/z*");

  result.ClearPatterns();
  URLPatternSet::CreateUnion(set1, set2, &result);
  EXPECT_EQ(expected, result);
}

TEST(URLPatternSetTest, Contains) {
  URLPatternSet set1;
  URLPatternSet set2;
  URLPatternSet empty_set;

  AddPattern(&set1, "http://www.google.com/*");
  AddPattern(&set1, "http://www.yahoo.com/*");

  AddPattern(&set2, "http://www.reddit.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_TRUE(set1.Contains(empty_set));
  EXPECT_FALSE(empty_set.Contains(set1));

  AddPattern(&set2, "http://www.yahoo.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_FALSE(set2.Contains(set1));

  AddPattern(&set2, "http://www.google.com/*");

  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_TRUE(set2.Contains(set1));

  // Note that this just checks pattern equality, and not if individual patterns
  // contain other patterns. For example:
  AddPattern(&set1, "http://*.reddit.com/*");
  EXPECT_FALSE(set1.Contains(set2));
  EXPECT_FALSE(set2.Contains(set1));
}

TEST(URLPatternSetTest, Duplicates) {
  URLPatternSet set1;
  URLPatternSet set2;

  AddPattern(&set1, "http://www.google.com/*");
  AddPattern(&set2, "http://www.google.com/*");

  AddPattern(&set1, "http://www.google.com/*");

  // The sets should still be equal after adding a duplicate.
  EXPECT_EQ(set2, set1);
}
