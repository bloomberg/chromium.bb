// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_CONTROLLER_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class LocationBarView;

// View controller for the location bar, which contains the omnibox and
// associated decorations.
@interface LocationBarViewController : UIViewController

@property(nonatomic, readonly, strong) LocationBarView* locationBarView;

// The frame of the omnibox, in its window's coordinate system.  Declared as a
// property to make it KVO compliant and therefore usable by
// the Broadcaster.
@property(nonatomic, readwrite, assign) CGRect omniboxFrame;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_VIEW_CONTROLLER_H_
