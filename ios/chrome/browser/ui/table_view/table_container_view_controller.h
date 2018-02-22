// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class ChromeTableViewController;

// TableContainerViewController contains a ChromeTableViewController and a
// Bottom Toolbar that interacts with the ChromeTableViewController.
@interface TableContainerViewController : UIViewController

- (instancetype)initWithTable:(ChromeTableViewController*)table
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The bottom toolbar owned by this ViewController.
@property(nonatomic, strong) UIView* bottomToolbar;

// UIBarButtonItem to be used on a Navigation Controller to dismiss this
// ViewController.
@property(nonatomic, strong, readonly) UIBarButtonItem* dismissButton;

// The ChromeTableViewController owned by this ViewController.
@property(nonatomic, strong) ChromeTableViewController* tableViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABLE_VIEW_TABLE_CONTAINER_VIEW_CONTROLLER_H_
