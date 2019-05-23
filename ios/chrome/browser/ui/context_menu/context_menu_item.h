// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_

#import <Foundation/Foundation.h>

#include "base/ios/block_types.h"

// Stores the title for each Context Menu selection and an associated
// block that will be executed if the user selects the context menu item.
@interface ContextMenuItem : NSObject

// Context Menu item title
@property(nonatomic, copy, readonly) NSString* title;

// Action block associated with the Context Menu item
@property(nonatomic, copy) ProceduralBlock action;

// Designated initializer for a new Context Menu Item object. |action| block
// may be nil.
- (instancetype)initWithTitle:(NSString*)title
                       action:(ProceduralBlock)action NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTEXT_MENU_CONTEXT_MENU_ITEM_H_
