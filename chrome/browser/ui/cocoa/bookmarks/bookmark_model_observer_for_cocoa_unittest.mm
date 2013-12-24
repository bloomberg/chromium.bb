// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#import "chrome/browser/ui/cocoa/bookmarks/bookmark_model_observer_for_cocoa.h"
#import "chrome/browser/ui/cocoa/cocoa_profile_test.h"
#include "chrome/test/base/testing_profile.h"

namespace {

class BookmarkModelObserverForCocoaTest : public CocoaProfileTest {
};


TEST_F(BookmarkModelObserverForCocoaTest, TestCallback) {
  BookmarkModel* model = BookmarkModelFactory::GetForProfile(profile());
  const BookmarkNode* node = model->AddURL(model->bookmark_bar_node(),
                                           0, base::ASCIIToUTF16("super"),
                                           GURL("http://www.google.com"));

  __block size_t pings = 0U;
  __block size_t deletions = 0U;

  BookmarkModelObserverForCocoa::ChangeCallback callback =
      ^(BOOL nodeWasDeleted) {
          ++pings;
          if (nodeWasDeleted)
            ++deletions;
      };

  scoped_ptr<BookmarkModelObserverForCocoa>
      observer(new BookmarkModelObserverForCocoa(model,
                                                 callback));
  observer->StartObservingNode(node);

  EXPECT_EQ(0U, pings);
  EXPECT_EQ(0U, deletions);

  model->SetTitle(node, base::ASCIIToUTF16("duper"));
  EXPECT_EQ(1U, pings);
  EXPECT_EQ(0U, deletions);

  model->SetURL(node, GURL("http://www.google.com/reader"));
  EXPECT_EQ(2U, pings);
  EXPECT_EQ(0U, deletions);

  model->Move(node, model->other_node(), 0);
  EXPECT_EQ(3U, pings);
  EXPECT_EQ(0U, deletions);

  model->Remove(node->parent(), 0);
  EXPECT_EQ(4U, pings);
  EXPECT_EQ(1U, deletions);
}

}  // namespace
