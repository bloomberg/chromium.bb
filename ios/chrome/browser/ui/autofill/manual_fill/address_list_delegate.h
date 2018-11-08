// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ADDRESS_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ADDRESS_LIST_DELEGATE_H_

// Delegate for actions in manual fallback's addresses list.
@protocol AddressListDelegate

// Dismisses the presented view controller and continues as pop over on iPads
// or above the keyboard elsewhere.
- (void)dismissPresentedViewController;

// Opens addresses settings.
- (void)openAddressSettings;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_ADDRESS_LIST_DELEGATE_H_
