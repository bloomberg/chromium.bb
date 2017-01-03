// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class InfoBarView;
class InfoBarViewDelegate;
namespace infobars {
class InfoBarDelegate;
}

// InfoBar for iOS acts as a UIViewController for InfoBarView.
@interface InfoBarController : NSObject

@property(nonatomic, readonly) InfoBarViewDelegate* delegate;

// Designated initializer.
- (instancetype)initWithDelegate:(InfoBarViewDelegate*)delegate
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Creates a view and lays out all the infobar elements in it. Will not add
// it as a subview yet. This method must be overriden in subclasses.
- (InfoBarView*)viewForDelegate:(infobars::InfoBarDelegate*)delegate
                          frame:(CGRect)bounds;

// Creates the view.
- (void)layoutForDelegate:(infobars::InfoBarDelegate*)delegate
                    frame:(CGRect)bounds;

// Detaches view from its delegate.
// After this function is called, no user interaction can be handled.
- (void)detachView;

// Returns the actual height in pixels of this infobar instance.
- (int)barHeight;

// Adjusts visible portion of this infobar.
- (void)onHeightsRecalculated:(int)newHeight;

// Removes the view.
- (void)removeView;

// Accesses the view.
- (InfoBarView*)view;

@end

#endif  // IOS_CHROME_BROWSER_INFOBARS_INFOBAR_CONTROLLER_H_
