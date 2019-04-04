// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_

#import <UIKit/UIKit.h>

// InfobarBannerPositioner contains methods used to position the InfobarBanner.
@protocol InfobarBannerPositioner

// Y axis value used to position the InfobarBanner.
- (CGFloat)bannerYPosition;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_BANNER_POSITIONER_H_
