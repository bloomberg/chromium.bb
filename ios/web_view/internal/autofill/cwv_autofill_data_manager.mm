// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/autofill/cwv_autofill_data_manager_internal.h"

#include "components/autofill/core/browser/personal_data_manager.h"

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

@end
