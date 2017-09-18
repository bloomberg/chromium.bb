// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"
#import "ios/chrome/browser/ui/tabs/requirements/tab_strip_constants.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class TabModel;
@protocol TabStripPresentation;

// Controller class for the tabstrip.  Manages displaying tabs and keeping the
// display in sync with the TabModel.  This controller is only instantiated on
// tablet.  The tab strip view itself is a subclass of UIScrollView, which
// manages scroll offsets and scroll animations.
@interface TabStripController : NSObject<BubbleViewAnchorPointProvider>

@property(nonatomic, assign) BOOL highlightsSelectedTab;
@property(nonatomic, readonly, retain) UIView* view;

@property(nonatomic, readonly, weak) id<BrowserCommands, ApplicationCommands>
    dispatcher;

// The duration to wait before starting tab strip animations. Used to
// synchronize animations.
@property(nonatomic, assign) NSTimeInterval animationWaitDuration;

// Used to check if the tabstrip is visible before starting an animation.
@property(nonatomic, assign) id<TabStripPresentation> presentationProvider;

// Designated initializer, |dispatcher| is not retained.
- (instancetype)initWithTabModel:(TabModel*)tabModel
                           style:(TabStripStyle)style
                      dispatcher:
                          (id<ApplicationCommands, BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
