// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@class UnifiedConsentViewController;

// Delegate protocol for UnifiedConsentViewController.
@protocol UnifiedConsentViewControllerDelegate<NSObject>

// Called when the user taps on the settings link.
- (void)unifiedConsentViewControllerDidTapSettingsLink:
    (UnifiedConsentViewController*)controller;

// Called when the user taps at |point| on the IdentityPickerView. |point| is in
// the window coordinates.
- (void)unifiedConsentViewControllerDidTapIdentityPickerView:
            (UnifiedConsentViewController*)controller
                                                     atPoint:(CGPoint)point;

// Called when the user scrolls down to the bottom (or when the view controller
// is loaded with no scroll needed).
- (void)unifiedConsentViewControllerDidReachBottom:
    (UnifiedConsentViewController*)controller;

// Called when the view appears.
- (void)unifiedConsentViewControllerViewDidAppear:
    (UnifiedConsentViewController*)controller;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_VIEW_CONTROLLER_DELEGATE_H_
