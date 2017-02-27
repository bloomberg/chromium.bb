// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_ACTIONS_H_
#define IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_ACTIONS_H_

// Protocol handling the actions sent in the
// ShippingAddressSelectionViewController.
@protocol ShippingAddressSelectionViewControllerActions

// Called when the user presses the return button.
- (void)onReturn;

@end

#endif  // IOS_CHROME_BROWSER_PAYMENTS_SHIPPING_ADDRESS_SELECTION_VIEW_CONTROLLER_ACTIONS_H_
