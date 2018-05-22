// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include <memory>

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"
#import "ios/web_view/internal/autofill/cwv_credit_card_internal.h"
#import "ios/web_view/public/cwv_autofill_data_manager_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CWVAutofillDataManager ()
// Called when WebViewPersonalDataManagerObserverBridge's
// |OnPersonalDataChanged| is invoked.
- (void)personalDataDidChange;

@end

namespace ios_web_view {
// C++ to ObjC bridge for PersonalDataManagerObserver.
class WebViewPersonalDataManagerObserverBridge
    : public autofill::PersonalDataManagerObserver {
 public:
  explicit WebViewPersonalDataManagerObserverBridge(
      CWVAutofillDataManager* data_manager)
      : data_manager_(data_manager){};
  ~WebViewPersonalDataManagerObserverBridge() override = default;

  // autofill::PersonalDataManagerObserver implementation.
  void OnPersonalDataChanged() override {
    [data_manager_ personalDataDidChange];
  }

  void OnInsufficientFormData() override {
    // Nop.
  }

 private:
  __weak CWVAutofillDataManager* data_manager_;
};
}  // namespace ios_web_view

@implementation CWVAutofillDataManager {
  autofill::PersonalDataManager* _personalDataManager;
  std::unique_ptr<ios_web_view::WebViewPersonalDataManagerObserverBridge>
      _personalDataManagerObserverBridge;
}

@synthesize delegate = _delegate;

- (instancetype)initWithPersonalDataManager:
    (autofill::PersonalDataManager*)personalDataManager {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
    _personalDataManagerObserverBridge = std::make_unique<
        ios_web_view::WebViewPersonalDataManagerObserverBridge>(self);
    _personalDataManager->AddObserver(_personalDataManagerObserverBridge.get());
  }
  return self;
}

- (void)dealloc {
  _personalDataManager->RemoveObserver(
      _personalDataManagerObserverBridge.get());
}

#pragma mark - Public Methods

- (NSArray<CWVAutofillProfile*>*)profiles {
  NSMutableArray* profiles = [NSMutableArray array];
  for (autofill::AutofillProfile* internalProfile :
       _personalDataManager->GetProfiles()) {
    CWVAutofillProfile* profile =
        [[CWVAutofillProfile alloc] initWithProfile:*internalProfile];
    [profiles addObject:profile];
  }
  return [profiles copy];
}

- (NSArray<CWVCreditCard*>*)creditCards {
  NSMutableArray* creditCards = [NSMutableArray array];
  for (autofill::CreditCard* internalCard :
       _personalDataManager->GetCreditCards()) {
    CWVCreditCard* creditCard =
        [[CWVCreditCard alloc] initWithCreditCard:*internalCard];
    [creditCards addObject:creditCard];
  }
  return [creditCards copy];
}

- (void)updateProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->UpdateProfile(*profile.internalProfile);
}

- (void)deleteProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->RemoveByGUID(profile.internalProfile->guid());
}

- (void)updateCreditCard:(CWVCreditCard*)creditCard {
  _personalDataManager->UpdateCreditCard(*creditCard.internalCard);
}

- (void)deleteCreditCard:(CWVCreditCard*)creditCard {
  _personalDataManager->RemoveByGUID(creditCard.internalCard->guid());
}

#pragma mark - Private Methods

- (void)personalDataDidChange {
  [_delegate autofillDataManagerDataDidChange:self];
}

@end
