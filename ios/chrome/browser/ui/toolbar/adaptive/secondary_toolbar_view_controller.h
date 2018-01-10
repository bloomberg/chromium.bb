// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ToolbarButtonFactory;

// The ViewController for the secondary toolbar.
@interface SecondaryToolbarViewController : UIViewController

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)buttonFactory;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_CONTROLLER_H_
