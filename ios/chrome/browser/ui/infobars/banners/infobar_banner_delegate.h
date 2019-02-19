// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate to handle InfobarBanner actions.
@protocol InfobarBannerDelegate

// Called when the InfobarBanner button was pressed.
- (void)bannerInfobarButtonWasPressed:(UIViewController*)sender;

// Asks the delegate to dismiss the InfobarBanner.
- (void)dismissInfobarBanner:(UIViewController*)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_DELEGATE_H_
