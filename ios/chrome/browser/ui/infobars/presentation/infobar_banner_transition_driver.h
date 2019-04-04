// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_

#import <UIKit/UIKit.h>

@protocol InfobarBannerPositioner;

// The transition delegate used to present an InfobarBanner.
@interface InfobarBannerTransitionDriver
    : NSObject <UIViewControllerTransitioningDelegate>

// Delegate used to position the InfobarBanner.
@property(nonatomic, assign) id<InfobarBannerPositioner> bannerPositioner;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_TRANSITION_DRIVER_H_
