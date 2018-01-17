// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"

@protocol ApplicationCommands;
@protocol BrowserCommands;
@class ToolbarButton;
@class ToolbarButtonVisibilityConfiguration;
@protocol ToolbarCommands;
@class ToolbarTabGridButton;
@class ToolbarToolsMenuButton;
@class ToolbarConfiguration;

// ToolbarButton Factory protocol to create ToolbarButton objects with certain
// style and configuration, depending of the implementation.
// A dispatcher is used to send the commands associated with the buttons.
@interface ToolbarButtonFactory : NSObject

- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, assign, readonly) ToolbarStyle style;
// Configuration object for styling. It is used by the factory to set the style
// of the buttons title.
@property(nonatomic, strong, readonly)
    ToolbarConfiguration* toolbarConfiguration;
// Dispatcher used to initialize targets for the buttons.
@property(nonatomic, weak)
    id<ApplicationCommands, BrowserCommands, ToolbarCommands>
        dispatcher;
// Configuration object for the visibility of the buttons.
@property(nonatomic, strong)
    ToolbarButtonVisibilityConfiguration* visibilityConfiguration;

// Back ToolbarButton.
- (ToolbarButton*)backButton;
// Forward ToolbarButton to be displayed on the leading side of the toolbar.
- (ToolbarButton*)leadingForwardButton;
// Forward ToolbarButton to be displayed on the trailing side of the toolbar.
- (ToolbarButton*)trailingForwardButton;
// Tab Grid ToolbarButton.
- (ToolbarTabGridButton*)tabGridButton;
// Tab Switcher Strip ToolbarButton.
- (ToolbarButton*)tabSwitcherStripButton;
// Tab Switcher Grid ToolbarButton.
- (ToolbarButton*)tabSwitcherGridButton;
// Tools Menu ToolbarButton.
- (ToolbarToolsMenuButton*)toolsMenuButton;
// Share ToolbarButton.
- (ToolbarButton*)shareButton;
// Reload ToolbarButton.
- (ToolbarButton*)reloadButton;
// Stop ToolbarButton.
- (ToolbarButton*)stopButton;
// Bookmark ToolbarButton.
- (ToolbarButton*)bookmarkButton;
// VoiceSearch ToolbarButton.
- (ToolbarButton*)voiceSearchButton;
// ContractToolbar ToolbarButton.
- (ToolbarButton*)contractButton;
// ToolbarButton to focus the omnibox.
- (ToolbarButton*)omniboxButton;
// LocationBar LeadingButton. Currently used for the incognito icon when the
// Toolbar is expanded on incognito mode. It can return nil.
- (ToolbarButton*)locationBarLeadingButton;

// Returns images for Voice Search in an array representing the NORMAL/PRESSED
// state
- (NSArray<UIImage*>*)voiceSearchImages;
// Returns images for TTS in an array representing the NORMAL/PRESSED states.
- (NSArray<UIImage*>*)TTSImages;

@end

#endif  // IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_BUTTON_FACTORY_H_
