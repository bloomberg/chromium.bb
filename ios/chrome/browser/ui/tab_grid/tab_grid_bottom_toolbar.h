// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_BOTTOM_TOOLBAR_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_BOTTOM_TOOLBAR_H_

#import <UIKit/UIKit.h>

@class TabGridNewTabButton;

// Bottom toolbar for TabGrid. In horizontal-compact and vertical-regular screen
// size, the toolbar has a translucent background and shows 3 buttons, with
// newTabButton located in the middle. For other screen sizes, the toolbar has a
// transparent background and only shows the newTabButton on the right.
@interface TabGridBottomToolbar : UIToolbar
// These components are publicly available to allow the user to set their
// contents, visibility and actions.
@property(nonatomic, strong, readonly) UIBarButtonItem* leadingButton;
@property(nonatomic, strong, readonly) UIBarButtonItem* trailingButton;
// Clang does not allow property getters to start with the reserved word "new",
// but provides a workaround. The getter must be set before the property is
// declared.
- (TabGridNewTabButton*)newTabButton __attribute__((objc_method_family(none)));
@property(nonatomic, strong, readonly) TabGridNewTabButton* newTabButton;

// Hides components and uses a black background color for tab grid transition
// animation.
- (void)hide;
// Recovers the normal appearance for tab grid transition animation.
- (void)show;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_TAB_GRID_BOTTOM_TOOLBAR_H_
