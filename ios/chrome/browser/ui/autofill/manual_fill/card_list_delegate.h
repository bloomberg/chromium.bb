// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_

namespace autofill {
class CreditCard;
}  // namespace autofill

// Delegate for actions in manual fallback's cards list.
@protocol CardListDelegate

// Dismisses the presented view controller and continues as pop over on iPads
// or above the keyboard else.
- (void)dismissPresentedViewController;

// Opens cards settings.
- (void)openCardSettings;

// Open credit card unlock, through CVC, prompt.
- (void)requestFullCreditCard:(const autofill::CreditCard&)card;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_MANUAL_FILL_CARD_LIST_DELEGATE_H_
