// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/suggestions/suggestions_article_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/collection_view_item.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SuggestionsArticleItemTest, CellIsConfigured) {
  NSString* title = @"testTitle";
  NSString* subtitle = @"testSubtitle";
  UIImage* image = [[UIImage alloc] init];
  SuggestionsArticleItem* item =
      [[SuggestionsArticleItem alloc] initWithType:0
                                             title:title
                                          subtitle:subtitle
                                             image:image];
  SuggestionsArticleCell* cell = [[[item cellClass] alloc] init];
  EXPECT_TRUE([cell isMemberOfClass:[SuggestionsArticleCell class]]);

  [item configureCell:cell];
  EXPECT_EQ(title, cell.titleLabel.text);
  EXPECT_EQ(subtitle, cell.subtitleLabel.text);
  EXPECT_EQ(image, cell.imageView.image);
}

}  // namespace
