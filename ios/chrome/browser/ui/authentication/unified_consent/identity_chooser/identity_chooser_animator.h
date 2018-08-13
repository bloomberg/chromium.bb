// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_

#import <UIKit/UIKit.h>

// Animator, taking care of the animation of the IdentityChooser.
@interface IdentityChooserAnimator
    : NSObject<UIViewControllerAnimatedTransitioning>

@property(nonatomic, assign) BOOL appearing;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_IDENTITY_CHOOSER_IDENTITY_CHOOSER_ANIMATOR_H_
