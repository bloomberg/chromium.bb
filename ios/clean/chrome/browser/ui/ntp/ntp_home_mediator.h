// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_MEDIATOR_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/url_loader.h"

@protocol TabCommands;

// A mediator object that sets an NTP home view controller's appearance based
// on various data sources.
@interface NTPHomeMediator : NSObject<UrlLoader, OmniboxFocuser>

// The dispatcher for this mediator.
@property(nonatomic, weak) id<TabCommands> dispatcher;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_NTP_NTP_HOME_MEDIATOR_H_
