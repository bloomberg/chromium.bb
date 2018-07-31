// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_ITEM_H_
#define IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_ITEM_H_

#import <Foundation/Foundation.h>

// Model object representing an item in a grid.
@interface GridItem : NSObject

// Create an item with |identifier|, which cannot be nil.
- (instancetype)initWithIdentifier:(NSString*)identifier
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@property(nonatomic, readonly) NSString* identifier;
@property(nonatomic, copy) NSString* title;
@property(nonatomic, assign) BOOL hidesTitle;
@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_GRID_GRID_GRID_ITEM_H_
