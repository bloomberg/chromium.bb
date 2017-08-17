// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/bubble/bubble_view_anchor_point_provider.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@protocol FullScreenControllerDelegate;
@class TabModel;
@class TabView;

namespace TabStrip {
enum Style { kStyleDark, kStyleIncognito };

// Returns the background color of the tabstrip view.
UIColor* BackgroundColor();

}  // namespace TabStrip

// Notification when the tab strip will start an animation.
extern NSString* const kWillStartTabStripTabAnimation;

// Notifications when the user starts and ends a drag operation.
extern NSString* const kTabStripDragStarted;
extern NSString* const kTabStripDragEnded;

// Controller class for the tabstrip.  Manages displaying tabs and keeping the
// display in sync with the TabModel.  This controller is only instantiated on
// tablet.  The tab strip view itself is a subclass of UIScrollView, which
// manages scroll offsets and scroll animations.
@interface TabStripController : NSObject<BubbleViewAnchorPointProvider>

@property(nonatomic, assign) BOOL highlightsSelectedTab;
@property(nonatomic, readonly, retain) UIView* view;

@property(nonatomic, readonly, weak) id<BrowserCommands> dispatcher;

// Used to check if the tabstrip is visible before starting an animation.
@property(nonatomic, assign) id<FullScreenControllerDelegate>
    fullscreenDelegate;

// Designated initializer.
- (instancetype)initWithTabModel:(TabModel*)tabModel
                           style:(TabStrip::Style)style
                      dispatcher:(id<BrowserCommands>)dispatcher
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Records metrics for the given action.
- (IBAction)recordUserMetrics:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
