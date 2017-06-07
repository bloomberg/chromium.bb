// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/payments/payment_request_mediator.h"

#include "base/strings/sys_string_conversions.h"
#include "components/signin/core/browser/signin_manager.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"

@implementation PaymentRequestMediator {
  ios::ChromeBrowserState* _browserState;
}

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState {
  DCHECK(browserState);
  self = [super init];
  if (self) {
    _browserState = browserState;
  }
  return self;
}

- (NSString*)authenticatedAccountName {
  const SigninManager* signinManager =
      ios::SigninManagerFactory::GetForBrowserStateIfExists(_browserState);
  if (signinManager && signinManager->IsAuthenticated()) {
    return base::SysUTF8ToNSString(
        signinManager->GetAuthenticatedAccountInfo().email);
  }
  return nil;
}

@end
