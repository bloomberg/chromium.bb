// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol InfobarBannerDelegate;

// TODO(crbug.com/911864): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@interface InfobarBannerViewController : UIViewController

- (instancetype)initWithDelegate:(id<InfobarBannerDelegate>)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

// The message displayed by this InfobarBanner.
@property(nonatomic, copy) NSString* messageText;

// The button text displayed by this InfobarBanner.
@property(nonatomic, copy) NSString* buttonText;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_BANNERS_INFOBAR_BANNER_VIEW_CONTROLLER_H_
