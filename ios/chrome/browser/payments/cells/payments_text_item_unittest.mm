// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/payments_text_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that the text label and the image are set properly after a call to
// |configureCell:|.
TEST(PaymentsTextItemTest, TextLabelAndImage) {
  PaymentsTextItem* item = [[PaymentsTextItem alloc] initWithType:0];

  NSString* text = @"Lorem ipsum dolor sit amet";
  UIImage* image = ios_internal::CollectionViewTestImage();

  item.text = text;
  item.image = image;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[PaymentsTextCell class]]);

  PaymentsTextCell* paymentsTextCell = cell;
  EXPECT_FALSE(paymentsTextCell.textLabel.text);
  EXPECT_FALSE(paymentsTextCell.imageView.image);

  [item configureCell:paymentsTextCell];
  EXPECT_NSEQ(text, paymentsTextCell.textLabel.text);
  EXPECT_NSEQ(image, paymentsTextCell.imageView.image);
}

}  // namespace
