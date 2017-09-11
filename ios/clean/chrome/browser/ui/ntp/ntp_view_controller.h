// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/clean/chrome/browser/ui/ntp/ntp_consumer.h"

@protocol NTPCommands;

// View controller that displays a new tab page.
@interface NTPViewController : UIViewController<NTPConsumer>

// The dispatcher for this view controller
@property(nonatomic, weak) id<NTPCommands> dispatcher;

// Setting this property adds an Incognito panel.
@property(nonatomic, strong) UIViewController* incognitoViewController;

// Setting this property adds a Chrome Home panel.
@property(nonatomic, strong) UIViewController* homeViewController;

// Setting this property adds a bookmarks panel on iPad or present a bookmarks
// panel on iPhone.
@property(nonatomic, strong) UIViewController* bookmarksViewController;

// Setting this property adds a recent tabs panel on iPad or present a bookmarks
// panel on iPhone.
@property(nonatomic, strong) UIViewController* recentTabsViewController;

// The panel currently selected.
@property(nonatomic, assign) ntp_home::PanelIdentifier selectedNTPPanel;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_VIEW_CONTROLLER_H_
