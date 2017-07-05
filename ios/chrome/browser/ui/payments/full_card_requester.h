// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_
#define IOS_CHROME_BROWSER_UI_PAYMENTS_FULL_CARD_REQUESTER_H_

#include <string>

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "components/payments/core/payment_instrument.h"

namespace autofill {
class AutofillManager;
class CreditCard;
}  // namespace autofill

namespace ios {
class ChromeBrowserState;
}  // namespace ios

@protocol FullCardRequesterConsumer

// Called when a credit card has been successfully unmasked. Should be
// called wth method name (e.g., "visa") and json-serialized details.
- (void)fullCardRequestDidSucceedWithMethodName:(const std::string&)methodName
                             stringifiedDetails:
                                 (const std::string&)stringifiedDetails;

@end

// Receives the full credit card details. Also displays the unmask prompt UI.
class FullCardRequester
    : public payments::PaymentInstrument::Delegate,
      public autofill::payments::FullCardRequest::UIDelegate,
      public base::SupportsWeakPtr<FullCardRequester> {
 public:
  FullCardRequester(id<FullCardRequesterConsumer> consumer,
                    UIViewController* base_view_controller,
                    ios::ChromeBrowserState* browser_state);

  void GetFullCard(
      const autofill::CreditCard& card,
      autofill::AutofillManager* autofill_manager,
      base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
          result_delegate);

  // payments::PaymentInstrument::Delegate:
  void OnInstrumentDetailsReady(
      const std::string& method_name,
      const std::string& stringified_details) override;
  void OnInstrumentDetailsError() override{};

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
