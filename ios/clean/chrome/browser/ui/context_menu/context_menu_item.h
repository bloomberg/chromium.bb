// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_

#import <Foundation/Foundation.h>
#include <vector>

// Object encapsulating configuration information for an item in a context menu.
@interface ContextMenuItem : NSObject

// ContextMenuItems should be created through the factory method.
- (instancetype)init NS_UNAVAILABLE;

// Create a new item with |title| and |commands|.
+ (instancetype)itemWithTitle:(NSString*)title
                      command:(SEL)command
              commandOpensTab:(BOOL)commandOpensTab;

// The title associated with the item. This is usually the text the user will
// see.
@property(nonatomic, readonly, copy) NSString* title;

// The ContextMenuCommand to be dispatched with menu's ContextMenuContext.
@property(nonatomic, readonly, assign) SEL command;

// Returns whether dispatching |command| will open a new tab.
@property(nonatomic, readonly, assign) BOOL commandOpensTab;

@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_
