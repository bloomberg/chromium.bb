// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ConsentBumpViewControllerDelegate;

// View Controller handling the ConsentBump screen.
@interface ConsentBumpViewController : UIViewController

// Delegate for the view controller.
@property(nonatomic, weak) id<ConsentBumpViewControllerDelegate> delegate;

// View controller displayed as child view controller for this ViewController.
@property(nonatomic, strong) UIViewController* contentViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_CONSENT_BUMP_CONSENT_BUMP_VIEW_CONTROLLER_H_
