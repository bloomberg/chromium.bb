// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/subtree_set.h"

#include "testing/gtest/include/gtest/gtest.h"

#define FPL(path) base::FilePath(FILE_PATH_LITERAL(path))

namespace sync_file_system {

TEST(SubtreeSetTest, InsertAndErase) {
  SubtreeSet subtrees;

  EXPECT_EQ(0u, subtrees.size());
  EXPECT_TRUE(subtrees.Insert(FPL("/a/b/c")));
  EXPECT_FALSE(subtrees.Insert(FPL("/a/b")));
  EXPECT_FALSE(subtrees.Insert(FPL("/a/b/c")));
  EXPECT_FALSE(subtrees.Insert(FPL("/a/b/c/d")));
  EXPECT_TRUE(subtrees.Insert(FPL("/a/b/d")));
  EXPECT_FALSE(subtrees.Insert(FPL("/")));

  EXPECT_EQ(2u, subtrees.size());

  EXPECT_FALSE(subtrees.Erase(FPL("/")));
  EXPECT_FALSE(subtrees.Erase(FPL("/a")));
  EXPECT_TRUE(subtrees.Erase(FPL("/a/b/c")));

  EXPECT_EQ(1u, subtrees.size());

  EXPECT_TRUE(subtrees.Insert(FPL("/a/b/c/d")));

  EXPECT_EQ(2u, subtrees.size());
}

}  // namespace sync_file_system
