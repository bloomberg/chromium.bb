// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SAFE_MODE_SAFE_MODE_VIEW_CONTROLLER_H_
#define IOS_CHROME_APP_SAFE_MODE_SAFE_MODE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"

@class PrimaryActionButton;

// A protocol required by delegates of the SafeModeViewController.
@protocol SafeModeViewControllerDelegate
@required
// Tell delegate to attempt to start the browser.
- (void)startBrowserFromSafeMode;
@end

@interface SafeModeViewController : UIViewController {
 @private
  base::WeakNSProtocol<id<SafeModeViewControllerDelegate>> delegate_;
  base::scoped_nsobject<UIView> innerView_;
  base::scoped_nsobject<PrimaryActionButton> startButton_;
  base::scoped_nsobject<UILabel> uploadDescription_;
  base::scoped_nsobject<UIProgressView> uploadProgress_;
  base::scoped_nsobject<NSDate> uploadStartTime_;
  base::scoped_nsobject<NSTimer> uploadTimer_;
}

- (id)initWithDelegate:(id<SafeModeViewControllerDelegate>)delegate;

// Returns |YES| when the safe mode UI has information to show.
+ (BOOL)hasSuggestions;

@end

#endif  // IOS_CHROME_APP_SAFE_MODE_SAFE_MODE_VIEW_CONTROLLER_H_
