// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol OmniboxFocuser;
@class TabModel;
@protocol UrlLoader;
@protocol WebToolbarDelegate;

namespace ios {
class ChromeBrowserState;
}

// Google centric new tab page.
@interface GoogleLandingController : NSObject<LogoAnimationControllerOwnerOwner,
                                              NewTabPagePanelProtocol,
                                              ToolbarOwner>

// Initialization method.
- (id)initWithLoader:(id<UrlLoader>)loader
          browserState:(ios::ChromeBrowserState*)browserState
               focuser:(id<OmniboxFocuser>)focuser
    webToolbarDelegate:(id<WebToolbarDelegate>)webToolbarDelegate
              tabModel:(TabModel*)tabModel;

// Get the maximum number of sites shown.
+ (NSUInteger)maxSitesShown;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_
