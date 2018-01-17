// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_

#import <UIKit/UIKit.h>

@class ToolbarButton;
@class ToolbarButtonFactory;
@class ToolbarToolsMenuButton;

// Protocol defining the interface for interacting with a view of the adaptive
// toolbar.
@protocol AdaptiveToolbarView<NSObject>

// Factory used to create the buttons.
@property(nonatomic, strong) ToolbarButtonFactory* buttonFactory;

// Property to get all the buttons in this view.
@property(nonatomic, strong, readonly) NSArray<ToolbarButton*>* allButtons;

// Button to display the tools menu.
@property(nonatomic, strong, readonly) ToolbarToolsMenuButton* toolsMenuButton;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_ADAPTIVE_ADAPTIVE_TOOLBAR_VIEW_H_
