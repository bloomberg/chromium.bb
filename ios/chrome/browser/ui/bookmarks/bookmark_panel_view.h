// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PANEL_VIEW_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PANEL_VIEW_H_

#import <UIKit/UIKit.h>

@class BookmarkPanelView;

@protocol BookmarkPanelViewDelegate
// The menu is going to be programmatically shown or hidden.
- (void)bookmarkPanelView:(BookmarkPanelView*)view
             willShowMenu:(BOOL)showMenu
    withAnimationDuration:(CGFloat)duration;
// |visibility| is 1 when the menu is fully visible, and 0 when the menu is
// fully hidden.
// This callback is invoked frequently during a user-driven animation.
// This callback is not invoked for programmatic animations.
// Every user-driven animation is guaranteed to be followed by a programmatic
// animation.
- (void)bookmarkPanelView:(BookmarkPanelView*)view
    updatedMenuVisibility:(CGFloat)visibility;
@end

// This view holds a content view, and a menu view.  The menu sits off the
// screen to the left. The user can pan right on the screen to drag the menu
// view onto the content view.
@interface BookmarkPanelView : UIView

// Designated initializer.
// The menu has the same height as the frame.
- (id)initWithFrame:(CGRect)frame menuViewWidth:(CGFloat)width;

// If a user-driven animation is in progress, these methods have no effect.
// Calling these methods will invoke delegate callbacks.
- (void)showMenuAnimated:(BOOL)animated;
- (void)hideMenuAnimated:(BOOL)animated;

// Even if this method returns NO, the menu can still be in the process of a
// programmatic animation.
- (BOOL)userDrivenAnimationInProgress;

@property(nonatomic, assign) id<BookmarkPanelViewDelegate> delegate;

// These views should not be modified directly. Instead, subviews should be
// added to each to create the desired UI.
@property(nonatomic, retain, readonly) UIView* contentView;
@property(nonatomic, retain, readonly) UIView* menuView;

// Whether the menu is being shown. If a user-driven animation is in progress,
// this property reflects the state of the menu at the beginning of the
// animation.
@property(nonatomic, assign, readonly) BOOL showingMenu;

// This method is used to enable or disable swiping in the menu from the left.
- (void)enableSideSwiping:(BOOL)enable;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PANEL_VIEW_H_
