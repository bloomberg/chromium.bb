// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_mediator.h"

#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/authentication/consent_bump/consent_bump_consumer.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ConsentBumpMediator

@synthesize consumer = _consumer;

- (void)updateConsumerForConsentBumpScreen:
    (ConsentBumpScreen)consentBumpScreen {
  switch (consentBumpScreen) {
    case ConsentBumpScreenUnifiedConsent:
      [self.consumer setPrimaryButtonTitle:l10n_util::GetNSString(
                                               IDS_IOS_CONSENT_BUMP_IM_IN)];
      [self.consumer setSecondaryButtonTitle:l10n_util::GetNSString(
                                                 IDS_IOS_CONSENT_BUMP_MORE)];
      break;

    case ConsentBumpScreenPersonalization:
      [self.consumer
          setPrimaryButtonTitle:l10n_util::GetNSString(IDS_ACCNAME_OK)];
      [self.consumer
          setSecondaryButtonTitle:l10n_util::GetNSString(IDS_ACCNAME_BACK)];
      [self consumerCanProceed];
      break;
  }
}

- (void)consumerCanProceed {
  [self.consumer showPrimaryButton];
}

@end
