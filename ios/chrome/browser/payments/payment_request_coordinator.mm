// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/payments/payment_request_coordinator.h"

#include <unordered_set>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/payments/payment_request.h"
#include "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/autofill/card_unmask_prompt_view_bridge.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
using ::payment_request_util::GetBasicCardResponseFromAutofillCreditCard;
using ::payment_request_util::GetPaymentAddressFromAutofillProfile;
}  // namespace

// The unmask prompt UI for Payment Request.
class PRCardUnmaskPromptViewBridge
    : public autofill::CardUnmaskPromptViewBridge {
 public:
  explicit PRCardUnmaskPromptViewBridge(
      autofill::CardUnmaskPromptController* controller,
      UIViewController* base_view_controller)
      : autofill::CardUnmaskPromptViewBridge(controller),
        base_view_controller_(base_view_controller) {}

  // autofill::CardUnmaskPromptView:
  void Show() override {
    view_controller_.reset(
        [[CardUnmaskPromptViewController alloc] initWithBridge:this]);
    [base_view_controller_ presentViewController:view_controller_
                                        animated:YES
                                      completion:nil];
  };

 private:
  __weak UIViewController* base_view_controller_;
  DISALLOW_COPY_AND_ASSIGN(PRCardUnmaskPromptViewBridge);
};

// Receives the full credit card details. Also displays the unmask prompt UI.
class FullCardRequester
    : public autofill::payments::FullCardRequest::ResultDelegate,
      public autofill::payments::FullCardRequest::UIDelegate,
      public base::SupportsWeakPtr<FullCardRequester> {
 public:
  explicit FullCardRequester(PaymentRequestCoordinator* owner,
                             UIViewController* base_view_controller,
                             ios::ChromeBrowserState* browser_state)
      : owner_(owner),
        base_view_controller_(base_view_controller),
        unmask_controller_(browser_state->GetPrefs(),
                           browser_state->IsOffTheRecord()) {}

  void GetFullCard(autofill::CreditCard* card,
                   autofill::AutofillManager* autofill_manager) {
    DCHECK(card);
    DCHECK(autofill_manager);
    autofill_manager->GetOrCreateFullCardRequest()->GetFullCard(
        *card, autofill::AutofillClient::UNMASK_FOR_PAYMENT_REQUEST,
        AsWeakPtr(), AsWeakPtr());
  }

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(const autofill::CreditCard& card,
                                  const base::string16& cvc) override {
    [owner_ fullCardRequestDidSucceedWithCard:card CVC:cvc];
  }

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestFailed() override {
    // No action is required here. PRCardUnmaskPromptViewBridge manages its own
    // life cycle. When the prompt is explicitly dismissed via tapping the close
    // button (either in presence or absence of an error), the unmask prompt
    // dialog pops itself and the user is back to the Payment Request UI.
  }

  // payments::FullCardRequest::UIDelegate:
  void ShowUnmaskPrompt(
      const autofill::CreditCard& card,
      autofill::AutofillClient::UnmaskCardReason reason,
      base::WeakPtr<autofill::CardUnmaskDelegate> delegate) override {
    unmask_controller_.ShowPrompt(
        // PRCardUnmaskPromptViewBridge manages its own lifetime.
        new PRCardUnmaskPromptViewBridge(&unmask_controller_,
                                         base_view_controller_),
        card, reason, delegate);
  }

  // payments::FullCardRequest::UIDelegate:
  void OnUnmaskVerificationResult(
      autofill::AutofillClient::PaymentsRpcResult result) override {
    unmask_controller_.OnVerificationResult(result);
  }

 private:
  __weak PaymentRequestCoordinator* owner_;
  __weak UIViewController* base_view_controller_;
  autofill::CardUnmaskPromptControllerImpl unmask_controller_;

  DISALLOW_COPY_AND_ASSIGN(FullCardRequester);
};

@implementation PaymentRequestCoordinator {
  UINavigationController* _navigationController;
  PaymentRequestViewController* _viewController;
  PaymentRequestErrorCoordinator* _errorCoordinator;
  PaymentItemsDisplayCoordinator* _itemsDisplayCoordinator;
  ShippingAddressSelectionCoordinator* _shippingAddressSelectionCoordinator;
  ShippingOptionSelectionCoordinator* _shippingOptionSelectionCoordinator;
  PaymentMethodSelectionCoordinator* _methodSelectionCoordinator;

  // Receiver of the full credit card details. Also displays the unmask prompt
  // UI.
  std::unique_ptr<FullCardRequester> _fullCardRequester;

  // The selected shipping address, pending approval from the page.
  autofill::AutofillProfile* _pendingShippingAddress;
}

@synthesize paymentRequest = _paymentRequest;
@synthesize autofillManager = _autofillManager;
@synthesize browserState = _browserState;
@synthesize pageFavicon = _pageFavicon;
@synthesize pageTitle = _pageTitle;
@synthesize pageHost = _pageHost;
@synthesize delegate = _delegate;

- (void)start {
  _viewController = [[PaymentRequestViewController alloc]
      initWithPaymentRequest:_paymentRequest];
  [_viewController setPageFavicon:_pageFavicon];
  [_viewController setPageTitle:_pageTitle];
  [_viewController setPageHost:_pageHost];
  [_viewController setDelegate:self];
  [_viewController loadModel];

  _navigationController = [[UINavigationController alloc]
      initWithRootViewController:_viewController];
  [_navigationController setNavigationBarHidden:YES];

  [[self baseViewController] presentViewController:_navigationController
                                          animated:YES
                                        completion:nil];
}

- (void)stop {
  [[_navigationController presentingViewController]
      dismissViewControllerAnimated:YES
                         completion:nil];
  _itemsDisplayCoordinator = nil;
  _shippingAddressSelectionCoordinator = nil;
  _shippingOptionSelectionCoordinator = nil;
  _methodSelectionCoordinator = nil;
  _errorCoordinator = nil;
  _viewController = nil;
  _navigationController = nil;
}

- (void)sendPaymentResponse {
  DCHECK(_paymentRequest->selected_credit_card());
  autofill::CreditCard* card = _paymentRequest->selected_credit_card();
  _fullCardRequester = base::MakeUnique<FullCardRequester>(
      self, _navigationController, _browserState);
  _fullCardRequester->GetFullCard(card, _autofillManager);
}

- (void)fullCardRequestDidSucceedWithCard:(const autofill::CreditCard&)card
                                      CVC:(const base::string16&)cvc {
  web::PaymentResponse paymentResponse;

  paymentResponse.method_name =
      base::ASCIIToUTF16(autofill::data_util::GetPaymentRequestData(card.type())
                             .basic_card_payment_type);

  paymentResponse.details =
      GetBasicCardResponseFromAutofillCreditCard(*_paymentRequest, card, cvc);

  if (_paymentRequest->payment_options().request_shipping) {
    autofill::AutofillProfile* shippingAddress =
        _paymentRequest->selected_shipping_profile();
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a shipping address.
    DCHECK(shippingAddress);
    paymentResponse.shipping_address =
        GetPaymentAddressFromAutofillProfile(*shippingAddress);

    web::PaymentShippingOption* shippingOption =
        _paymentRequest->selected_shipping_option();
    DCHECK(shippingOption);
    paymentResponse.shipping_option = shippingOption->id;
  }

  if (_paymentRequest->payment_options().request_payer_name) {
    autofill::AutofillProfile* contactInfo =
        _paymentRequest->selected_contact_profile();
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a contact info.
    DCHECK(contactInfo);
    paymentResponse.payer_name =
        contactInfo->GetInfo(autofill::AutofillType(autofill::NAME_FULL),
                             GetApplicationContext()->GetApplicationLocale());
  }

  if (_paymentRequest->payment_options().request_payer_email) {
    autofill::AutofillProfile* contactInfo =
        _paymentRequest->selected_contact_profile();
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a contact info.
    DCHECK(contactInfo);
    paymentResponse.payer_email =
        contactInfo->GetRawInfo(autofill::EMAIL_ADDRESS);
  }

  if (_paymentRequest->payment_options().request_payer_phone) {
    autofill::AutofillProfile* contactInfo =
        _paymentRequest->selected_contact_profile();
    // TODO(crbug.com/602666): User should get here only if they have selected
    // a contact info.
    DCHECK(contactInfo);
    paymentResponse.payer_phone =
        contactInfo->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);
  }

  _viewController.view.userInteractionEnabled = NO;
  [_viewController setPending:YES];
  [_viewController loadModel];
  [[_viewController collectionView] reloadData];

  [_delegate paymentRequestCoordinator:self
         didConfirmWithPaymentResponse:paymentResponse];
}

- (void)updatePaymentDetails:(web::PaymentDetails)paymentDetails {
  BOOL totalValueChanged =
      (_paymentRequest->payment_details().total != paymentDetails.total);
  _paymentRequest->set_payment_details(paymentDetails);

  if (_paymentRequest->shipping_options().empty()) {
    // Display error in the shipping address/option selection view.
    if (_shippingAddressSelectionCoordinator) {
      _paymentRequest->set_selected_shipping_profile(nil);
      [_shippingAddressSelectionCoordinator stopSpinnerAndDisplayError];
    } else if (_shippingOptionSelectionCoordinator) {
      [_shippingOptionSelectionCoordinator stopSpinnerAndDisplayError];
    }
    // Update the payment request summary view.
    [_viewController loadModel];
    [[_viewController collectionView] reloadData];
  } else {
    // Update the payment summary section.
    [_viewController
        updatePaymentSummaryWithTotalValueChanged:totalValueChanged];

    if (_shippingAddressSelectionCoordinator) {
      // Set the selected shipping address and update the selected shipping
      // address in the payment request summary view.
      _paymentRequest->set_selected_shipping_profile(_pendingShippingAddress);
      _pendingShippingAddress = nil;
      [_viewController updateSelectedShippingAddressUI];

      // Dismiss the shipping address selection view.
      [_shippingAddressSelectionCoordinator stop];
      _shippingAddressSelectionCoordinator = nil;
    } else if (_shippingOptionSelectionCoordinator) {
      // Update the selected shipping option in the payment request summary
      // view. The updated selection is already reflected in |_paymentRequest|.
      [_viewController updateSelectedShippingOptionUI];

      // Dismiss the shipping option selection view.
      [_shippingOptionSelectionCoordinator stop];
      _shippingOptionSelectionCoordinator = nil;
    }
  }
}

- (void)displayErrorWithCallback:(ProceduralBlock)callback {
  _errorCoordinator = [[PaymentRequestErrorCoordinator alloc]
      initWithBaseViewController:_navigationController];
  [_errorCoordinator setCallback:callback];
  [_errorCoordinator setDelegate:self];

  [_errorCoordinator start];
}

#pragma mark - PaymentRequestViewControllerDelegate

- (void)paymentRequestViewControllerDidCancel:
    (PaymentRequestViewController*)controller {
  [_delegate paymentRequestCoordinatorDidCancel:self];
}

- (void)paymentRequestViewControllerDidConfirm:
    (PaymentRequestViewController*)controller {
  [self sendPaymentResponse];
}

- (void)paymentRequestViewControllerDidSelectPaymentSummaryItem:
    (PaymentRequestViewController*)controller {
  _itemsDisplayCoordinator = [[PaymentItemsDisplayCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_itemsDisplayCoordinator setPaymentRequest:_paymentRequest];
  [_itemsDisplayCoordinator setDelegate:self];

  [_itemsDisplayCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectShippingAddressItem:
    (PaymentRequestViewController*)controller {
  _shippingAddressSelectionCoordinator =
      [[ShippingAddressSelectionCoordinator alloc]
          initWithBaseViewController:_viewController];
  [_shippingAddressSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_shippingAddressSelectionCoordinator setDelegate:self];

  [_shippingAddressSelectionCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectShippingOptionItem:
    (PaymentRequestViewController*)controller {
  _shippingOptionSelectionCoordinator =
      [[ShippingOptionSelectionCoordinator alloc]
          initWithBaseViewController:_viewController];
  [_shippingOptionSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_shippingOptionSelectionCoordinator setDelegate:self];

  [_shippingOptionSelectionCoordinator start];
}

- (void)paymentRequestViewControllerDidSelectPaymentMethodItem:
    (PaymentRequestViewController*)controller {
  _methodSelectionCoordinator = [[PaymentMethodSelectionCoordinator alloc]
      initWithBaseViewController:_viewController];
  [_methodSelectionCoordinator setPaymentRequest:_paymentRequest];
  [_methodSelectionCoordinator setDelegate:self];

  [_methodSelectionCoordinator start];
}

#pragma mark - PaymentRequestErrorCoordinatorDelegate

- (void)paymentRequestErrorCoordinatorDidDismiss:
    (PaymentRequestErrorCoordinator*)coordinator {
  ProceduralBlock callback = coordinator.callback;

  [_errorCoordinator stop];
  _errorCoordinator = nil;

  if (callback)
    callback();
}

#pragma mark - PaymentItemsDisplayCoordinatorDelegate

- (void)paymentItemsDisplayCoordinatorDidReturn:
    (PaymentItemsDisplayCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_itemsDisplayCoordinator stop];
  _itemsDisplayCoordinator = nil;
}

- (void)paymentItemsDisplayCoordinatorDidConfirm:
    (PaymentItemsDisplayCoordinator*)coordinator {
  [self sendPaymentResponse];
}

#pragma mark - ShippingAddressSelectionCoordinatorDelegate

- (void)shippingAddressSelectionCoordinator:
            (ShippingAddressSelectionCoordinator*)coordinator
                   didSelectShippingAddress:
                       (autofill::AutofillProfile*)shippingAddress {
  _pendingShippingAddress = shippingAddress;
  DCHECK(shippingAddress);
  web::PaymentAddress address =
      GetPaymentAddressFromAutofillProfile(*shippingAddress);
  [_delegate paymentRequestCoordinator:self didSelectShippingAddress:address];
}

- (void)shippingAddressSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_shippingAddressSelectionCoordinator stop];
  _shippingAddressSelectionCoordinator = nil;
}

#pragma mark - ShippingOptionSelectionCoordinatorDelegate

- (void)shippingOptionSelectionCoordinator:
            (ShippingOptionSelectionCoordinator*)coordinator
                   didSelectShippingOption:
                       (web::PaymentShippingOption*)shippingOption {
  [_delegate paymentRequestCoordinator:self
               didSelectShippingOption:*shippingOption];
}

- (void)shippingOptionSelectionCoordinatorDidReturn:
    (ShippingAddressSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_shippingOptionSelectionCoordinator stop];
  _shippingOptionSelectionCoordinator = nil;
}

#pragma mark - PaymentMethodSelectionCoordinatorDelegate

- (void)paymentMethodSelectionCoordinator:
            (PaymentMethodSelectionCoordinator*)coordinator
                   didSelectPaymentMethod:(autofill::CreditCard*)creditCard {
  _paymentRequest->set_selected_credit_card(creditCard);

  [_viewController updateSelectedPaymentMethodUI];

  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

- (void)paymentMethodSelectionCoordinatorDidReturn:
    (PaymentMethodSelectionCoordinator*)coordinator {
  // Clear the 'Updated' label on the payment summary item, if there is one.
  [_viewController updatePaymentSummaryWithTotalValueChanged:NO];

  [_methodSelectionCoordinator stop];
  _methodSelectionCoordinator = nil;
}

@end
