// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include "components/autofill/core/browser/personal_data_manager.h"
#import "ios/web_view/internal/autofill/cwv_autofill_profile_internal.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CWVAutofillDataManager {
  autofill::PersonalDataManager* _personalDataManager;
}

- (instancetype)initWithPersonalDataManager:
    (autofill::PersonalDataManager*)personalDataManager {
  self = [super init];
  if (self) {
    _personalDataManager = personalDataManager;
  }
  return self;
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

- (void)updateProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->UpdateProfile(*profile.internalProfile);
}

- (void)deleteProfile:(CWVAutofillProfile*)profile {
  _personalDataManager->RemoveByGUID(profile.internalProfile->guid());
}

@end
