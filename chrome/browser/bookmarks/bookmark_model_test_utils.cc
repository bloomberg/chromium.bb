// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_test_utils.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "testing/gtest/include/gtest/gtest.h"

// static
void BookmarkModelTestUtils::AssertNodesEqual(const BookmarkNode* expected,
                                              const BookmarkNode* actual,
                                              bool check_ids) {
  ASSERT_TRUE(expected);
  ASSERT_TRUE(actual);
  if (check_ids)
    EXPECT_EQ(expected->id(), actual->id());
  EXPECT_EQ(expected->GetTitle(), actual->GetTitle());
  EXPECT_EQ(expected->type(), actual->type());
  EXPECT_TRUE(expected->date_added() == actual->date_added());
  if (expected->is_url()) {
    EXPECT_EQ(expected->url(), actual->url());
  } else {
    EXPECT_TRUE(expected->date_folder_modified() ==
                actual->date_folder_modified());
    ASSERT_EQ(expected->child_count(), actual->child_count());
    for (int i = 0; i < expected->child_count(); ++i)
      AssertNodesEqual(expected->GetChild(i), actual->GetChild(i), check_ids);
  }
}

// static
void BookmarkModelTestUtils::AssertModelsEqual(BookmarkModel* expected,
                                               BookmarkModel* actual,
                                               bool check_ids) {
  AssertNodesEqual(expected->bookmark_bar_node(),
                   actual->bookmark_bar_node(),
                   check_ids);
  AssertNodesEqual(expected->other_node(),
                   actual->other_node(),
                   check_ids);
  AssertNodesEqual(expected->mobile_node(),
                   actual->mobile_node(),
                   check_ids);
}
