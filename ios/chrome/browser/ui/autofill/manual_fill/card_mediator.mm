// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"

#include <vector>

#include "base/metrics/user_metrics.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/credit_card.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/action_cell.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_consumer.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credit_card.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/credit_card_form.h"
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

- (const autofill::CreditCard*)findCreditCardfromGUID:(NSString*)GUID {
  for (autofill::CreditCard* card : self.cards) {
    NSString* cppGUID =
        base::SysUTF16ToNSString(base::ASCIIToUTF16(card->guid()));
    if ([cppGUID isEqualToString:GUID])
      return card;
  }
  return nil;
}

- (void)reloadWithCards:(std::vector<autofill::CreditCard*>)cards {
  self.cards = cards;
  if (self.consumer) {
    [self postCardsToConsumer];
    [self postActionsToConsumer];
  }
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
    ManualFillCreditCard* manualFillCreditCard =
        [[ManualFillCreditCard alloc] initWithCreditCard:*card];
    auto item =
        [[ManualFillCardItem alloc] initWithCreditCard:manualFillCreditCard
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
  NSString* manageCreditCardsTitle =
      l10n_util::GetNSString(IDS_IOS_MANUAL_FALLBACK_MANAGE_CREDIT_CARDS);
  __weak __typeof(self) weakSelf = self;
  auto manageCreditCardsItem = [[ManualFillActionItem alloc]
      initWithTitle:manageCreditCardsTitle
             action:^{
               base::RecordAction(base::UserMetricsAction(
                   "ManualFallback_CreditCard_OpenManageCreditCard"));
               [weakSelf.navigationDelegate openCardSettings];
             }];
  manageCreditCardsItem.accessibilityIdentifier =
      manual_fill::ManageCardsAccessibilityIdentifier;
  [self.consumer presentActions:@[ manageCreditCardsItem ]];
}

#pragma mark - FullCardRequestResultDelegateObserving

- (void)onFullCardRequestSucceeded:(const autofill::CreditCard&)card {
  // Credit card are not shown as 'Secure'.
  ManualFillCreditCard* manualFillCreditCard =
      [[ManualFillCreditCard alloc] initWithCreditCard:card];
  // Don't replace the locked card with the unlocked one, so the user will
  // have to unlock it again, if needed.
  [self.contentDelegate userDidPickContent:manualFillCreditCard.number
                             passwordField:NO
                             requiresHTTPS:YES];
}

- (void)onFullCardRequestFailed {
  // This is called on user cancelling, so there's nothing to do here.
}

@end
