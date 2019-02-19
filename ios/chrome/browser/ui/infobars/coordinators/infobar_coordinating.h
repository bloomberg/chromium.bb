// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_

// InfobarCoordinating defines common methods for all Infobar Coordinators.
@protocol InfobarCoordinating

// YES if the Coordinator has been started.
@property(nonatomic, assign) BOOL started;

// The Coordinator BannerViewController.
- (UIViewController*)bannerViewController;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_COORDINATORS_INFOBAR_COORDINATING_H_
