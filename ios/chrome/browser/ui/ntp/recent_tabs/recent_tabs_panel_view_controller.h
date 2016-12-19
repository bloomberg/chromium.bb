// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

namespace ios {
class ChromeBrowserState;
}

@protocol UrlLoader;

// UIViewController wrapper for RecentTabsPanelController for modal display.
@interface RecentTabsPanelViewController : UIViewController

- (instancetype)initWithLoader:(id<UrlLoader>)loader
                  browserState:(ios::ChromeBrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

+ (UIViewController*)controllerToPresentForBrowserState:
                         (ios::ChromeBrowserState*)browserState
                                                 loader:(id<UrlLoader>)loader;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_RECENT_TABS_RECENT_TABS_PANEL_VIEW_CONTROLLER_H_
