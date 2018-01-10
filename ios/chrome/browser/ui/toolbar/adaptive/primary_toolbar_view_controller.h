// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol ApplicationCommands;
@class ToolbarButtonFactory;
@protocol BrowserCommands;

// ViewController for the primary toolbar. In an adaptive toolbar paradigm, this
// is the toolbar always presented.
@interface PrimaryToolbarViewController : UIViewController

- (instancetype)initWithButtonFactory:(ToolbarButtonFactory*)buttonFactory;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;

// Dispatcher for the ViewController.
@property(nonatomic, weak) id<ApplicationCommands, BrowserCommands> dispatcher;

// Sets the location bar view, containing the omnibox.
- (void)setLocationBarView:(UIView*)locationBarView;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_PRIMARY_TOOLBAR_VIEW_CONTROLLER_H_
