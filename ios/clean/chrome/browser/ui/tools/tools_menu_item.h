// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_ITEM_H_
#define IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_ITEM_H_

#import <Foundation/Foundation.h>

// Object used as a ToolsMenu StackView item.
@interface ToolsMenuItem : NSObject
@property(nonatomic, copy) NSString* title;
@property(nonatomic) SEL action;
@property(nonatomic) BOOL enabled;
@end

#endif  // IOS_CLEAN_CHROME_BROWSER_UI_TOOLS_TOOLS_MENU_ITEM_H_
