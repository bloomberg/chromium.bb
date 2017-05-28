// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_

@class ContextMenuContext;

// Commands relating to the context menu.
@protocol ContextMenuCommands<NSObject>
@optional
// Executes |context|'s script.
- (void)executeContextMenuScript:(ContextMenuContext*)context;
// Opens |context|'s link in a new tab.
- (void)openContextMenuLinkInNewTab:(ContextMenuContext*)context;
// Opens |context|'s link in a new Incognito tab.
- (void)openContextMenuLinkInNewIncognitoTab:(ContextMenuContext*)context;
// Copies |context|'s link to the paste board.
- (void)copyContextMenuLink:(ContextMenuContext*)context;
// Saves's the image at |context|'s image URL to the camera roll.
- (void)saveContextMenuImage:(ContextMenuContext*)context;
// Opens the image at |context|'s image URL .
- (void)openContextMenuImage:(ContextMenuContext*)context;
// Opens the image at |context|'s image URL  in a new tab.
- (void)openContextMenuImageInNewTab:(ContextMenuContext*)context;
// Hides the context menu UI.
- (void)hideContextMenu:(ContextMenuContext*)context;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_COMMANDS_CONTEXT_MENU_COMMANDS_H_
