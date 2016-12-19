// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/page_info_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that the labels and image are set properly after a call to
// |configureCell:|.
TEST(PageInfoItemTest, TextLabels) {
  PageInfoItem* item = [[PageInfoItem alloc] initWithType:0];

  UIImage* pageFavicon = ios_internal::CollectionViewTestImage();
  NSString* pageTitle = @"The Greatest Website Ever";
  NSString* pageHost = @"www.greatest.example.com";

  item.pageFavicon = pageFavicon;
  item.pageTitle = pageTitle;
  item.pageHost = pageHost;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[PageInfoCell class]]);

  PageInfoCell* pageInfoCell = cell;
  EXPECT_FALSE(pageInfoCell.pageTitleLabel.text);
  EXPECT_FALSE(pageInfoCell.pageHostLabel.text);
  EXPECT_FALSE(pageInfoCell.pageFaviconView.image);

  [item configureCell:pageInfoCell];
  EXPECT_NSEQ(pageTitle, pageInfoCell.pageTitleLabel.text);
  EXPECT_NSEQ(pageHost, pageInfoCell.pageHostLabel.text);
  EXPECT_NSEQ(pageFavicon, pageInfoCell.pageFaviconView.image);
}

}  // namespace
