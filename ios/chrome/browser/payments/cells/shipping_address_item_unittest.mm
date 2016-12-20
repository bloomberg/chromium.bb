// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/cells/shipping_address_item.h"

#import "ios/chrome/browser/ui/collection_view/cells/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Tests that the labels are set properly after a call to |configureCell:|.
TEST(ShippingAddressItemTest, TextLabels) {
  ShippingAddressItem* item = [[ShippingAddressItem alloc] initWithType:0];

  NSString* name = @"Jon Doe";
  NSString* address = @"123 Main St, Anytown, USA";
  NSString* phoneNumber = @"1234567890";

  item.name = name;
  item.address = address;
  item.phoneNumber = phoneNumber;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[ShippingAddressCell class]]);

  ShippingAddressCell* shippingAddressCell = cell;
  EXPECT_FALSE(shippingAddressCell.nameLabel.text);
  EXPECT_FALSE(shippingAddressCell.addressLabel.text);
  EXPECT_FALSE(shippingAddressCell.phoneNumberLabel.text);

  [item configureCell:shippingAddressCell];
  EXPECT_NSEQ(name, shippingAddressCell.nameLabel.text);
  EXPECT_NSEQ(address, shippingAddressCell.addressLabel.text);
  EXPECT_NSEQ(phoneNumber, shippingAddressCell.phoneNumberLabel.text);
}

}  // namespace
