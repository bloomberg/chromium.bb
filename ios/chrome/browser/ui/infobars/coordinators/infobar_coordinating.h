// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_

@class InfobarBannerTransitionDriver;
@class InfobarModalTransitionDriver;

// InfobarCoordinating defines common methods for all Infobar Coordinators.
@protocol InfobarCoordinating

// YES if the Coordinator has been started.
@property(nonatomic, assign) BOOL started;

// The transition delegate used by the Coordinator to present the InfobarBanner.
// nil if no Banner is being presented.
@property(nonatomic, strong)
    InfobarBannerTransitionDriver* bannerTransitionDriver;

// The transition delegate used by the Coordinator to present the InfobarModal.
// nil if no Modal is being presented.
@property(nonatomic, strong)
    InfobarModalTransitionDriver* modalTransitionDriver;

// The Coordinator's BannerViewController, can be nil.
- (UIViewController*)bannerViewController;

// Present the InfobarBanner for this Infobar using |baseViewController|.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// baseViewController will be set on init.
- (void)presentInfobarBannerFrom:(UIViewController*)baseViewController;

// Present the InfobarModal for this Infobar using |baseViewController|.
// TODO(crbug.com/927064): Once we create the coordinators in the UI Hierarchy
// baseViewController will be set on init.
- (void)presentInfobarModalFrom:(UIViewController*)baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_
