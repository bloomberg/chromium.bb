// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/payments/contact_info_selection_coordinator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/payments/core/strings_util.h"
#include "components/strings/grit/components_strings.h"
#include "ios/chrome/browser/payments/payment_request.h"
#import "ios/chrome/browser/payments/payment_request_util.h"
#include "ios/chrome/browser/ui/payments/contact_info_selection_mediator.h"
#include "ios/chrome/browser/ui/payments/payment_request_selector_view_controller.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The delay in nano seconds before notifying the delegate of the selection.
// This is here to let the user get a visual feedback of the selection before
// this view disappears.
const int64_t kDelegateNotificationDelayInNanoSeconds = 0.2 * NSEC_PER_SEC;
}  // namespace

@interface ContactInfoSelectionCoordinator ()

@property(nonatomic, strong)
    PaymentRequestSelectorViewController* viewController;

@property(nonatomic, strong) ContactInfoSelectionMediator* mediator;

// Called when the user selects a contact profile. The cell is checked, the
// UI is locked so that the user can't interact with it, then the delegate is
// notified.
- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)contactProfile;

@end

@implementation ContactInfoSelectionCoordinator

@synthesize paymentRequest = _paymentRequest;
@synthesize delegate = _delegate;
@synthesize viewController = _viewController;
@synthesize mediator = _mediator;

- (void)start {
  self.mediator = [[ContactInfoSelectionMediator alloc]
      initWithPaymentRequest:self.paymentRequest];

  self.viewController = [[PaymentRequestSelectorViewController alloc] init];
  self.viewController.title =
      l10n_util::GetNSString(IDS_PAYMENT_REQUEST_CONTACT_INFO_SECTION_NAME);
  self.viewController.delegate = self;
  self.viewController.dataSource = self.mediator;
  [self.viewController loadModel];

  DCHECK(self.baseViewController.navigationController);
  [self.baseViewController.navigationController
      pushViewController:self.viewController
                animated:YES];
}

- (void)stop {
  [self.baseViewController.navigationController popViewControllerAnimated:YES];
  self.viewController = nil;
  self.mediator = nil;
}

#pragma mark - PaymentRequestSelectorViewControllerDelegate

- (void)paymentRequestSelectorViewController:
            (PaymentRequestSelectorViewController*)controller
                        didSelectItemAtIndex:(NSUInteger)index {
  // Update the data source with the selection.
  self.mediator.selectedItemIndex = index;

  DCHECK(index < self.paymentRequest->contact_profiles().size());
  [self delayedNotifyDelegateOfSelection:self.paymentRequest
                                             ->contact_profiles()[index]];
}

- (void)paymentRequestSelectorViewControllerDidFinish:
    (PaymentRequestSelectorViewController*)controller {
  [self.delegate contactInfoSelectionCoordinatorDidReturn:self];
}

- (void)paymentRequestSelectorViewControllerDidSelectAddItem:
    (PaymentRequestSelectorViewController*)controller {
  // TODO(crbug.com/602666): Display contact info editor.
}

#pragma mark - Helper methods

- (void)delayedNotifyDelegateOfSelection:
    (autofill::AutofillProfile*)contactProfile {
  self.viewController.view.userInteractionEnabled = NO;
  __weak ContactInfoSelectionCoordinator* weakSelf = self;
  dispatch_after(
      dispatch_time(DISPATCH_TIME_NOW, kDelegateNotificationDelayInNanoSeconds),
      dispatch_get_main_queue(), ^{
        [weakSelf.delegate contactInfoSelectionCoordinator:weakSelf
                                   didSelectContactProfile:contactProfile];
      });
}

@end
