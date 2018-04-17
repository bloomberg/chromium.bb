// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_MEDIATOR_H_

#import <Foundation/Foundation.h>

@class ChromeIdentity;
@class UnifiedConsentViewController;

// A mediator object that monitor updates of the selecte chrome identity, and
// updates the UnifiedConsentViewController.
@interface UnifiedConsentMediator : NSObject

// Identity selected by the user to sign-in.
@property(nonatomic) ChromeIdentity* selectedIdentity;

- (instancetype)initWithUnifiedConsentViewController:
    (UnifiedConsentViewController*)viewController NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts this mediator.
- (void)start;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_MEDIATOR_H_
