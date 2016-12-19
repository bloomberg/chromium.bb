// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/mac/foundation_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#import "ios/chrome/browser/payments/shipping_address_selection_view_controller.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_text_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_controller_test.h"
#include "ios/chrome/grit/ios_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ShippingAddressSelectionViewControllerTest
    : public CollectionViewControllerTest {
 protected:
  CollectionViewController* NewController() override NS_RETURNS_RETAINED {
    return [[ShippingAddressSelectionViewController alloc] init];
  }

  ShippingAddressSelectionViewController* ShippingAddressSelectionController() {
    return base::mac::ObjCCastStrict<ShippingAddressSelectionViewController>(
        controller());
  }
};

// Tests that the correct number of items are displayed after loading the model
// and that the correct item appears to be selected.
TEST_F(ShippingAddressSelectionViewControllerTest, TestModel) {
  CreateController();
  CheckController();
  CheckTitleWithId(IDS_IOS_PAYMENT_REQUEST_SHIPPING_ADDRESS_SELECTION_TITLE);

  std::unique_ptr<autofill::AutofillProfile> profile1(
      new autofill::AutofillProfile());
  std::unique_ptr<autofill::AutofillProfile> profile2(
      new autofill::AutofillProfile());

  std::vector<autofill::AutofillProfile*> shippingAddresses;
  shippingAddresses.push_back(profile1.get());
  shippingAddresses.push_back(profile2.get());

  [ShippingAddressSelectionController() setShippingAddresses:shippingAddresses];
  [ShippingAddressSelectionController()
      setSelectedShippingAddress:shippingAddresses[0]];
  [ShippingAddressSelectionController() loadModel];

  ASSERT_EQ(1, NumberOfSections());
  // One item for each of shipping addresses plus 1 for the add button.
  EXPECT_EQ(shippingAddresses.size() + 1,
            static_cast<unsigned int>(NumberOfItemsInSection(0)));

  // The first address should appear to be selected.
  CollectionViewTextItem* item = GetCollectionViewItem(0, 0);
  EXPECT_EQ(MDCCollectionViewCellAccessoryCheckmark, item.accessoryType);
  item = GetCollectionViewItem(0, 1);
  EXPECT_EQ(MDCCollectionViewCellAccessoryNone, item.accessoryType);
}

}  // namespace
