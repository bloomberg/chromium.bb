// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_MOCK_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_MOCK_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace autofill {

class AutofillProfile;
class CreditCard;

class MockNewCreditCardBubbleController {
 public:
  MockNewCreditCardBubbleController();

  ~MockNewCreditCardBubbleController();

  void Show(scoped_ptr<CreditCard> new_card,
            scoped_ptr<AutofillProfile> billing_profile);

  const CreditCard* new_card() const { return new_card_.get(); }

  const AutofillProfile* billing_profile() const {
    return billing_profile_.get();
  }

  int bubbles_shown() const { return bubbles_shown_; }

 private:
  scoped_ptr<CreditCard> new_card_;
  scoped_ptr<AutofillProfile> billing_profile_;

  int bubbles_shown_;

  DISALLOW_COPY_AND_ASSIGN(MockNewCreditCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_MOCK_NEW_CREDIT_CARD_BUBBLE_CONTROLLER_H_
