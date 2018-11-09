// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/manual_fill/card_coordinator.h"

#include "base/memory/ref_counted.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#include "components/keyed_service/core/service_access_type.h"
#include "ios/chrome/browser/autofill/personal_data_manager_factory.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_list_delegate.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_mediator.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/card_view_controller.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/full_card_requester.h"
#import "ios/chrome/browser/ui/autofill/manual_fill/manual_fill_injection_handler.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/web/public/web_state/web_frame.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CardCoordinator ()<CardListDelegate>

// The view controller presented above the keyboard where the user can select
// one of their cards.
@property(nonatomic, strong) CardViewController* cardViewController;

// Fetches and filters the cards for the view controller.
@property(nonatomic, strong) ManualFillCardMediator* cardMediator;

// Requesters to unlock (through user CVC input) of server side credit cards.
@property(nonatomic, strong) ManualFillFullCardRequester* cardRequester;

@end

@implementation CardCoordinator

// Property tagged dynamic because it overrides super class delegate with and
// extension of the super delegate type (i.e. CardCoordinatorDelegate extends
// FallbackCoordinatorDelegate)
@dynamic delegate;

- (instancetype)
initWithBaseViewController:(UIViewController*)viewController
              browserState:(ios::ChromeBrowserState*)browserState
              webStateList:(WebStateList*)webStateList
          injectionHandler:(ManualFillInjectionHandler*)injectionHandler {
  self = [super initWithBaseViewController:viewController
                              browserState:browserState
                          injectionHandler:injectionHandler];
  if (self) {
    _cardViewController = [[CardViewController alloc] init];
    _cardViewController.contentInsetsAlwaysEqualToSafeArea = YES;

    autofill::PersonalDataManager* personalDataManager =
        autofill::PersonalDataManagerFactory::GetForBrowserState(browserState);

    std::vector<autofill::CreditCard*> cards =
        personalDataManager->GetCreditCardsToSuggest(true);

    _cardMediator = [[ManualFillCardMediator alloc] initWithCards:cards];
    _cardMediator.navigationDelegate = self;
    _cardMediator.contentDelegate = self.manualFillInjectionHandler;
    _cardMediator.consumer = _cardViewController;

    _cardRequester = [[ManualFillFullCardRequester alloc]
        initWithBrowserState:browserState
                webStateList:webStateList
              resultDelegate:_cardMediator];
  }
  return self;
}

#pragma mark - FallbackCoordinator

- (UIViewController*)viewController {
  return self.cardViewController;
}

#pragma mark - CardListDelegate

- (void)openCardSettings {
  __weak id<CardCoordinatorDelegate> delegate = self.delegate;
  [self dismissIfNecessaryThenDoCompletion:^{
    [delegate openCardSettings];
  }];
}

- (void)requestFullCreditCard:(const autofill::CreditCard&)card {
  __weak __typeof(self) weakSelf = self;
  // TODO(crbug.com/845472): resolve potential weak card here either using
  // std::unique or an Obj-C Creditcard object as proposed in another TODO.
  autofill::CreditCard cardCopy = card;
  [self dismissIfNecessaryThenDoCompletion:^{
    [weakSelf.cardRequester requestFullCreditCard:cardCopy
                           withBaseViewController:weakSelf.baseViewController];
  }];
}

@end
