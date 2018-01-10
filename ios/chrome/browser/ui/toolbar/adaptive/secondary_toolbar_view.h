// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class ToolbarButton;
@class ToolbarButtonFactory;
@class ToolbarToolsMenuButton;

// View for the secondary toolbar.
@interface SecondaryToolbarView : UIView

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Button to display the tools menu.
@property(nonatomic, strong, readonly) ToolbarToolsMenuButton* toolsMenuButton;
// Button to display the tab grid.
@property(nonatomic, strong, readonly) ToolbarButton* tabGridButton;
// Button to display the share menu.
@property(nonatomic, strong, readonly) ToolbarButton* shareButton;
// Button to focus the omnibox.
@property(nonatomic, strong, readonly) ToolbarButton* omniboxButton;
// Button to manage the bookmarks of this page.
@property(nonatomic, strong, readonly) ToolbarButton* bookmarksButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_SECONDARY_TOOLBAR_VIEW_H_
