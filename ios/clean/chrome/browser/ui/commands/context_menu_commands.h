// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_

@class ContextMenuDialogRequest;

// Commands relating to the context menu.
@protocol ContextMenuCommands<NSObject>
@optional
// Executes |request|'s script.
- (void)executeContextMenuScript:(ContextMenuDialogRequest*)request;
// Opens |request|'s link in a new tab.
- (void)openContextMenuLinkInNewTab:(ContextMenuDialogRequest*)request;
// Opens |request|'s link in a new Incognito tab.
- (void)openContextMenuLinkInNewIncognitoTab:(ContextMenuDialogRequest*)request;
// Copies |request|'s link to the paste board.
- (void)copyContextMenuLink:(ContextMenuDialogRequest*)request;
// Saves's the image at |request|'s image URL to the camera roll.
- (void)saveContextMenuImage:(ContextMenuDialogRequest*)request;
// Opens the image at |request|'s image URL .
- (void)openContextMenuImage:(ContextMenuDialogRequest*)request;
// Opens the image at |request|'s image URL  in a new tab.
- (void)openContextMenuImageInNewTab:(ContextMenuDialogRequest*)request;
@end

// Command protocol for dismissing context menus.
@protocol ContextMenuDismissalCommands<NSObject>

// Called after the user interaction with the context menu has been handled
// and the UI can be dismissed.
- (void)dismissContextMenu;

// Called after the user has selected an unavailable feature.  |featureName|
// should not be empty.
// TODO(crbug.com/760644): Delete after all context menu commands are
// implemented.
- (void)dismissContextMenuForUnavailableFeatureNamed:(NSString*)featureName;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_
