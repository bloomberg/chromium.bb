// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_BUTTON_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_TOOLBAR_CLEAN_TOOLBAR_BUTTON_FACTORY_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_style.h"

@class ToolbarButton;
@class ToolbarToolsMenuButton;
@class ToolbarConfiguration;

// ToolbarButton Factory protocol to create ToolbarButton objects with certain
// style and configuration, depending of the implementation.
@interface ToolbarButtonFactory : NSObject

- (instancetype)initWithStyle:(ToolbarStyle)style NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, assign, readonly) ToolbarStyle style;
// Configuration object for styling. It is used by the factory to set the style
// of the buttons title.
@property(nonatomic, strong, readonly)
    ToolbarConfiguration* toolbarConfiguration;

// Back ToolbarButton.
- (ToolbarButton*)backToolbarButton;
// Forward ToolbarButton.
- (ToolbarButton*)forwardToolbarButton;
// Tab Switcher Strip ToolbarButton.
- (ToolbarButton*)tabSwitcherStripToolbarButton;
// Tab Switcher Grid ToolbarButton.
- (ToolbarButton*)tabSwitcherGridToolbarButton;
// Tools Menu ToolbarButton.
- (ToolbarToolsMenuButton*)toolsMenuToolbarButton;
// Share ToolbarButton.
- (ToolbarButton*)shareToolbarButton;
// Reload ToolbarButton.
- (ToolbarButton*)reloadToolbarButton;
// Stop ToolbarButton.
- (ToolbarButton*)stopToolbarButton;
// Bookmark ToolbarButton.
- (ToolbarButton*)bookmarkToolbarButton;
// VoiceSearch ToolbarButton.
- (ToolbarButton*)voiceSearchButton;
// ContractToolbar ToolbarButton.
- (ToolbarButton*)contractToolbarButton;
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
