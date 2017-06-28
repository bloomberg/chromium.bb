// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"

namespace autofill {
class AutofillManager;
class CreditCard;
}  // namespace autofill

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@protocol FullCardRequesterConsumer

// Called when a credit card has been successfully unmasked. Note that |card|
// may be different from what passed to GetFullCard method of FullCardRequester,
// because CVC unmasking process may update the credit card number and
// expiration date.
- (void)fullCardRequestDidSucceedWithCard:(const autofill::CreditCard&)card
                         verificationCode:
                             (const base::string16&)verificationCode;

@end

// Receives the full credit card details. Also displays the unmask prompt UI.
class FullCardRequester
    : public autofill::payments::FullCardRequest::ResultDelegate,
      public autofill::payments::FullCardRequest::UIDelegate,
      public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester(id<FullCardRequesterConsumer> consumer,
                    UIViewController* base_view_controller,
                    ios::ChromeBrowserState* browser_state);

  void GetFullCard(autofill::CreditCard* card,
                   autofill::AutofillManager* autofill_manager);

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::CreditCard& card,
      const base::string16& verificationCode) override;
  void OnFullCardRequestFailed() override;

  // payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      autofill::AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override;
  void OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult result) override;

 private:
  __weak id<FullCardRequesterConsumer> consumer_;
  __weak UIViewController* base_view_controller_;
  autofill::CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequester);
};

#endif  // IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_
