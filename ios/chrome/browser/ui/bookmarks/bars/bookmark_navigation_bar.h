// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_NAVIGATION_BAR_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_NAVIGATION_BAR_H_

#import <UIKit/UIKit.h>

// The navigation bar on the bookmark home page.
@interface BookmarkNavigationBar : UIView

// The height that this view's content view will be instantiated at. This is
// independent of the status bar.
+ (CGFloat)expectedContentViewHeight;

// Designated initializer.
- (id)initWithFrame:(CGRect)outerFrame;

// Set the target/action of the cancel button.
- (void)setCancelTarget:(id)target action:(SEL)action;
// Set the target/action of the edit button.
- (void)setEditTarget:(id)target action:(SEL)action;
// Set the target/action of the menu button.
- (void)setMenuTarget:(id)target action:(SEL)action;
// Set the target/action of the back button.
- (void)setBackTarget:(id)target action:(SEL)action;
- (void)setTitle:(NSString*)title;

// Shows or hides the edit button.
- (void)showEditButtonWithAnimationDuration:(CGFloat)duration;
- (void)hideEditButtonWithAnimationDuration:(CGFloat)duration;
// |visibility| of 1 is fully visible, and 0 is fully hidden.
- (void)updateEditButtonVisibility:(CGFloat)visibility;

// The menu and back button are at the same place. Only one should show at the
// same time. By default, the menu button is showing.
- (void)showMenuButtonInsteadOfBackButton:(CGFloat)duration;
- (void)showBackButtonInsteadOfMenuButton:(CGFloat)duration;

@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BARS_BOOKMARK_NAVIGATION_BAR_H_
