// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/payments/full_card_requester.h"

#include "components/autofill/core/browser/autofill_manager.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"

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

FullCardRequester::FullCardRequester(id<FullCardRequesterConsumer> consumer,
                                     UIViewController* base_view_controller,
                                     ios::ChromeBrowserState* browser_state)
    : consumer_(consumer),
      base_view_controller_(base_view_controller),
      unmask_controller_(browser_state->GetPrefs(),
                         browser_state->IsOffTheRecord()) {}

void FullCardRequester::GetFullCard(
    const autofill::CreditCard& card,
    autofill::AutofillManager* autofill_manager,
    base::WeakPtr<autofill::payments::FullCardRequest::ResultDelegate>
        result_delegate) {
  DCHECK(autofill_manager);
  DCHECK(result_delegate);
  autofill_manager->GetOrCreateFullCardRequest()->GetFullCard(
      card, autofill::AutofillClient::UNMASK_FOR_PAYMENT_REQUEST,
      result_delegate, AsWeakPtr());
}

void FullCardRequester::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details) {
  [consumer_ fullCardRequestDidSucceedWithMethodName:method_name
                                  stringifiedDetails:stringified_details];
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
