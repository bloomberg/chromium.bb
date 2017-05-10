// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/payments/full_card_requester.h"

#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ios/chrome/browser/ui/payments/payment_request_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// The unmask prompt UI for Payment Request.
class PRCardUnmaskPromptViewBridge
    : public autofill::CardUnmaskPromptViewBridge {
 public:
  PRCardUnmaskPromptViewBridge(autofill::CardUnmaskPromptController* controller,
                               UIViewController* base_view_controller)
      : autofill::CardUnmaskPromptViewBridge(controller),
        base_view_controller_(base_view_controller) {}

  // autofill::CardUnmaskPromptView:
  void Show() override {
    view_controller_.reset(
        [[CardUnmaskPromptViewController alloc] initWithBridge:this]);
    [view_controller_ setModalPresentationStyle:UIModalPresentationFormSheet];
    [view_controller_
        setModalTransitionStyle:UIModalTransitionStyleCoverVertical];
    [base_view_controller_ presentViewController:view_controller_
                                        animated:YES
                                      completion:nil];
  };

 private:
  __weak UIViewController* base_view_controller_;
  DISALLOW_COPY_AND_ASSIGN(PRCardUnmaskPromptViewBridge);
};

}  // namespace

FullCardRequester::FullCardRequester(PaymentRequestCoordinator* owner,
                                     UIViewController* base_view_controller,
                                     ios::ChromeBrowserState* browser_state)
    : owner_(owner),
      base_view_controller_(base_view_controller),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

void FullCardRequester::GetFullCard(
    autofill::CreditCard* card,
    autofill::AutofillManager* autofill_manager) {
  DCHECK(card);
  DCHECK(autofill_manager);
  autofill_manager->GetOrCreateFullCardRequest()->GetFullCard(
      *card, autofill::AutofillClient::UNMASK_FOR_PAYMENT_REQUEST, AsWeakPtr(),
      AsWeakPtr());
}

void FullCardRequester::OnFullCardRequestSucceeded(
    const autofill::CreditCard& card,
    const base::string16& verificationCode) {
  [owner_ fullCardRequestDidSucceedWithCard:card
                           verificationCode:verificationCode];
}

void FullCardRequester::OnFullCardRequestFailed() {
  // No action is required here. PRCardUnmaskPromptViewBridge manages its own
  // life cycle. When the prompt is explicitly dismissed via tapping the close
  // button (either in presence or absence of an error), the unmask prompt
  // dialog pops itself and the user is back to the Payment Request UI.
}

void FullCardRequester::ShowUnmaskPrompt(
    const autofill::CreditCard& card,
    autofill::AutofillClient::UnmaskCardReason reason,
    base::WeakPtr<autofill::CardUnmaskDelegate> delegate) {
  unmask_controller_.ShowPrompt(
      // PRCardUnmaskPromptViewBridge manages its own lifetime.
      new PRCardUnmaskPromptViewBridge(&unmask_controller_,
                                       base_view_controller_),
      card, reason, delegate);
}

void FullCardRequester::OnUnmaskVerificationResult(
    autofill::AutofillClient::PaymentsRpcResult result) {
  unmask_controller_.OnVerificationResult(result);
}
