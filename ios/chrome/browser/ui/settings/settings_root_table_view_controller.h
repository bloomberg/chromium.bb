// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/material_components/app_bar_presenting.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_controller.h"

// SettingsRootTableViewController is a base class for integrating UITableViews
// into the Settings UI.  It handles the configuration and display of the MDC
// AppBar.
@interface SettingsRootTableViewController
    : ChromeTableViewController<AppBarPresenting>

- (instancetype)initWithStyle:(UITableViewStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)name
                         bundle:(NSBundle*)bundle NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)decoder NS_UNAVAILABLE;

#pragma mark UIScrollViewDelegate

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidScroll:(UIScrollView*)scrollView NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidEndDragging:(UIScrollView*)scrollView
                  willDecelerate:(BOOL)decelerate NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewDidEndDecelerating:(UIScrollView*)scrollView
    NS_REQUIRES_SUPER;

// Updates the MDCFlexibleHeader with changes to the table view scroll
// state. Must be called by subclasses if they override this method in order to
// maintain this functionality.
- (void)scrollViewWillEndDragging:(UIScrollView*)scrollView
                     withVelocity:(CGPoint)velocity
              targetContentOffset:(inout CGPoint*)targetContentOffset
    NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_BROWSER_UI_SETTINGS_SETTINGS_ROOT_TABLE_VIEW_CONTROLLER_H_
