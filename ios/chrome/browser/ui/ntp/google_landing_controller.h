// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "ios/chrome/browser/ui/ntp/google_landing_consumer.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_panel_protocol.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_owner.h"
#import "ios/public/provider/chrome/browser/voice/logo_animation_controller.h"

@protocol GoogleLandingDataSource;

// Google centric new tab page.
@interface GoogleLandingController
    : UIViewController<GoogleLandingConsumer,
                       LogoAnimationControllerOwnerOwner,
                       NewTabPagePanelProtocol,
                       ToolbarOwner>

@property(nonatomic, assign) id<GoogleLandingDataSource> dataSource;

@end

#endif  // IOS_CHROME_BROWSER_UI_NTP_GOOGLE_LANDING_CONTROLLER_H_
