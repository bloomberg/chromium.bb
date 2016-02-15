// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/reading_list_entry.h"

#include "testing/gtest/include/gtest/gtest.h"

TEST(ReadingListEntry, CompareIgnoreTitle) {
  const ReadingListEntry e1(GURL("http://example.com"), "bar");
  const ReadingListEntry e2(GURL("http://example.com"), "foo");

  EXPECT_EQ(e1, e2);
}

TEST(ReadingListEntry, CompareFailureIgnoreTitle) {
  const ReadingListEntry e1(GURL("http://example.com"), "bar");
  const ReadingListEntry e2(GURL("http://example.org"), "bar");

  EXPECT_FALSE(e1 == e2);
}

TEST(ReadingListEntry, CopyAreEquals) {
  const ReadingListEntry e1(GURL("http://example.com"), "bar");
  const ReadingListEntry e2(e1);

  EXPECT_EQ(e1, e2);
  EXPECT_EQ(e1.title(), e2.title());
}
