// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_TOOLBAR_INTERACTING_H_
#define IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_TOOLBAR_INTERACTING_H_

#import <UIKit/UIKit.h>

@class Tab;

// Protocol used by SideSwipe to interact with the toolbar.
@protocol SideSwipeToolbarInteracting

// Returns the toolbar view.
- (UIView*)toolbarView;
// Returns whether a swipe on the toolbar can start.
- (BOOL)canBeginToolbarSwipe;
// Returns a snapshot of the toolbar with the controls visibility adapted to
// |tab|.
- (UIImage*)toolbarSideSwipeSnapshotForTab:(Tab*)tab;

@end

#endif  // IOS_CHROME_BROWSER_UI_SIDE_SWIPE_SIDE_SWIPE_TOOLBAR_INTERACTING_H_
