// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/importer/importer_unittest_utils.h"

#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

void TestEqualBookmarkEntry(const ProfileWriter::BookmarkEntry& entry,
                            const BookmarkInfo& expected) {
  ASSERT_EQ(WideToUTF16Hack(expected.title), entry.title);
  ASSERT_EQ(expected.in_toolbar, entry.in_toolbar) << entry.title;
  ASSERT_EQ(expected.path_size, entry.path.size()) << entry.title;
  ASSERT_EQ(expected.url, entry.url.spec()) << entry.title;
  for (size_t i = 0; i < expected.path_size; ++i)
    ASSERT_EQ(WideToUTF16Hack(expected.path[i]), entry.path[i]) << entry.title;
}
