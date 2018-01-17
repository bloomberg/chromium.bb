// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class MDCProgressView;
@class ToolbarButton;
@class ToolbarButtonFactory;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;

// Protocol defining the interface for interacting with a view of the adaptive
// toolbar.
@protocol AdaptiveToolbarView<NSObject>

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Property to get all the buttons in this view.
@property(nonatomic, strong, readonly) NSArray<ToolbarButton*>* allButtons;

// Progress bar displayed below the toolbar.
@property(nonatomic, strong, readonly) MDCProgressView* progressBar;

// Button to navigate back.
@property(nonatomic, strong, readonly) ToolbarButton* backButton;
// Button to display the TabGrid.
@property(nonatomic, strong, readonly) ToolbarTabGridButton* tabGridButton;
// Button to stop the loading of the page.
@property(nonatomic, strong, readonly) ToolbarButton* stopButton;
// Button to reload the page.
@property(nonatomic, strong, readonly) ToolbarButton* reloadButton;
// Button to display the share menu.
@property(nonatomic, strong, readonly) ToolbarButton* shareButton;
// Button to manage the bookmarks of this page.
@property(nonatomic, strong, readonly) ToolbarButton* bookmarkButton;
// Button to display the tools menu.
@property(nonatomic, strong, readonly) ToolbarToolsMenuButton* toolsMenuButton;

// The following 2 properties are for the two buttons to navigate forward that
// are visible in various mutually exclusive configurations of the toolbar.
// Forward button when it's positioned on the leading side of the toolbar
// (relatively to the omnibox) | ← → [omnibox] ㉈ ⋮ |.
@property(nonatomic, strong, readonly) ToolbarButton* forwardLeadingButton;
// Forward button when it's positioned on the trailing side of the toolbar
// (relatively to the omnibox) | ← [omnibox] → |.
@property(nonatomic, strong, readonly) ToolbarButton* forwardTrailingButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_
