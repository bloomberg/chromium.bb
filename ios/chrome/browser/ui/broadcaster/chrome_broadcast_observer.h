// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
#define IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"

// Protocol collecting all of the methods that broadcast keys will trigger
// in an observer. Each key maps to a specific observer method as indicated.
// (this mapping is generated in the implementation of the Broadcaster class).
//
// All of the methods in this protocol *must* return void and take exactly one
// argument.
@protocol ChromeBroadcastObserver<NSObject>
@optional

#pragma mark - Tab strip UI

// Observer method for object that care about the current visibility of the tab
// strip.
- (void)broadcastTabStripVisible:(BOOL)visible;

#pragma mark - Scrolling events

// Observer method for objects that care about the current vertical (y-axis)
// scroll offset of the tab content area.
- (void)broadcastContentScrollOffset:(CGFloat)offset;

// Observer method for objects that care about whether the main content area is
// scrolling.
- (void)broadcastScrollViewIsScrolling:(BOOL)scrolling;

// Observer method for objects that care abotu whether the main content area is
// being dragged.  Note that if a drag ends with residual velocity, it's
// possible for |dragging| to be NO while |scrolling| is still YES.
- (void)broadcastScrollViewIsDragging:(BOOL)dragging;

#pragma mark - NTP UI

// Observer method for objects that care about the current panel selected on the
// NTP.
- (void)broadcastSelectedNTPPanel:(ntp_home::PanelIdentifier)panelIdentifier;

#pragma mark - Omnibox UI

// Observer method for objects that care about the current omnibox frame.  The
// given frame is in the window's coordinate system.
- (void)broadcastOmniboxFrame:(CGRect)frame;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROADCASTER_CHROME_BROADCAST_OBSERVER_H_
