// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class TabModel;
@class TabView;
@protocol FullScreenControllerDelegate;

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
@interface TabStripController : NSObject

@property(nonatomic, assign) BOOL highlightsSelectedTab;
@property(nonatomic, readonly, retain) UIView* view;
// YES if the tab strip will display the mode toggle switch. May be set to the
// same value repeatedly with no layout penalty.
@property(nonatomic, assign) BOOL hasModeToggleSwitch;

// YES if the tab strip will display the tab switcher toggle switch. May be set
// to the same value repeatedly with no layout penalty.
@property(nonatomic, assign) BOOL hasTabSwitcherToggleSwitch;

// May be nil if there is no mode toggle button.
@property(nonatomic, readonly, assign) UIButton* modeToggleButton;
// Used to check if the tabstrip is visible before starting an animation.
@property(nonatomic, assign) id<FullScreenControllerDelegate>
    fullscreenDelegate;

// Designated initializer.
- (instancetype)initWithTabModel:(TabModel*)tabModel
                           style:(TabStrip::Style)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Called when a tab is tapped.  |sender| should be a TabView.
- (IBAction)tabTapped:(id)sender;

// Records metrics for the given action.
- (IBAction)recordUserMetrics:(id)sender;

@end

#endif  // IOS_CHROME_BROWSER_UI_TABS_TAB_STRIP_CONTROLLER_H_
