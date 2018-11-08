// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"

#include <vector>

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_request_result_delegate_bridge.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_card_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_content_delegate.h"
#import "ios/chrome/browser/ui/list_model/list_model.h"
#import "ios/chrome/browser/ui/table_view/table_view_model.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/web/public/web_state/web_state.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace autofill {
class CreditCard;
}  // namespace autofill

namespace manual_fill {

NSString* const ManageCardsAccessibilityIdentifier =
    @"kManualFillManageCardsAccessibilityIdentifier";

}  // namespace manual_fill

@interface ManualFillCardMediator ()

// All available credit cards.
@property(nonatomic, assign) std::vector<autofill::CreditCard*> cards;

@end

@implementation ManualFillCardMediator

- (instancetype)initWithCards:(std::vector<autofill::CreditCard*>)cards {
  self = [super init];
  if (self) {
    _cards = cards;
  }
  return self;
}

- (void)setConsumer:(id<ManualFillCardConsumer>)consumer {
  if (consumer == _consumer) {
    return;
  }
  _consumer = consumer;
  [self postCardsToConsumer];
  [self postActionsToConsumer];
}

#pragma mark - Private

// Posts the cards to the consumer.
- (void)postCardsToConsumer {
  if (!self.consumer) {
    return;
  }

  NSMutableArray* items =
      [[NSMutableArray alloc] initWithCapacity:self.cards.size()];
  for (autofill::CreditCard* card : self.cards) {
    // TODO(crbug.com/845472): create a pure objc object instead.
    auto item =
        [[ManualFillCardItem alloc] initWithCreditCard:*card
                                       contentDelegate:self.contentDelegate
                                    navigationDelegate:self.navigationDelegate];
    [items addObject:item];
  }

  [self.consumer presentCards:items];
}

- (void)postActionsToConsumer {
  if (!self.consumer) {
    return;
  }
  // TODO(crbug.com/845472): implement.
  [self.consumer presentActions:@[]];
}

#pragma mark - FullCardRequestResultDelegateObserving

- (void)onFullCardRequestSucceeded:(const autofill::CreditCard&)card {
  // Credit card are not shown as 'Secure'.
  NSString* cardNumber =
      base::SysUTF16ToNSString(autofill::CreditCard::StripSeparators(
          card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  // TODO(crbug.com/845472): update userDidPickContent to have an isHttps
  // parameter.
  [self.contentDelegate userDidPickContent:cardNumber isSecure:NO];
}

- (void)onFullCardRequestFailed {
  // TODO(crbug.com/845472): handle failure to unlock card.
}

@end
