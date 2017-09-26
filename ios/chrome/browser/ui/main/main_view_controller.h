// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"

// A UIViewController that provides a property to maintain a single child view
// controller.
@interface MainViewController : UIViewController

// The view controller, if any, that is active.
@property(nonatomic, readonly, strong) UIViewController* activeViewController;

// Sets the active view controller, replacing any previously-active view
// controller.  The |completion| block is invoked once the new view controller
// is presented or added as a child.
- (void)setActiveViewController:(UIViewController*)activeViewController
                     completion:(ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_
