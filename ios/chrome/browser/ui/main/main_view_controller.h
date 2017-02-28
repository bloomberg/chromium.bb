// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

// A UIViewController that provides a property to maintain a single child view
// controller.
@interface MainViewController : UIViewController
// The child view controller, if any, that is active. Assigning to
// |activeViewController| will remove any previous active view controller.
@property(nonatomic, retain) UIViewController* activeViewController;
@end

#endif  // IOS_CHROME_BROWSER_UI_MAIN_MAIN_VIEW_CONTROLLER_H_
