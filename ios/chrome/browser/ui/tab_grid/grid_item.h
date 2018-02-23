// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_ITEM_H_

#import <Foundation/Foundation.h>

// Model object representing an item in a grid.
@interface GridItem : NSObject
@property(nonatomic, copy) NSString* identifier;
@property(nonatomic, copy) NSString* title;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_ITEM_H_
