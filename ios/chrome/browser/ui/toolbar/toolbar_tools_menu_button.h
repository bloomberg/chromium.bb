// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TOOLS_MENU_BUTTON_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TOOLS_MENU_BUTTON_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/fancy_ui/colored_button.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller_constants.h"

// ColoredButton specialization that updates the tint color when the tools menu
// is visible or when the reading list associated with |readingListModel|
// contains unread items.
// Draws and animates the icon of the button using UIBezierPaths.
@interface ToolbarToolsMenuButton : ColoredButton

// Initializes and returns a newly allocated TintedButton with the specified
// |frame| and the |style| of the toolbar it belongs to.
- (instancetype)initWithFrame:(CGRect)frame
                        style:(ToolbarControllerStyle)style
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;

- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;

// Informs the button that the Tools Menu's visibility is |toolsMenuVisible|.
- (void)setToolsMenuIsVisible:(BOOL)toolsMenuVisible;

// Notifies the button should alert user to the presence of reading list unseen
// items.
- (void)setReadingListContainsUnseenItems:(BOOL)readingListContainsUnseenItems;

// Triggers an animation on the button to draw the user's attention to the
// button.
- (void)triggerAnimation;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_TOOLBAR_TOOLS_MENU_BUTTON_H_
