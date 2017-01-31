// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_POSITION_CACHE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_POSITION_CACHE_H_

#import <Foundation/Foundation.h>

#include <stdint.h>

#include "base/macros.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_menu_item.h"

// Caches the user's position in the bookmark hierarchy navigator, as well as
// enough information to recreate the primary menu item.
@interface BookmarkPositionCache : NSObject<NSCoding>

@property(nonatomic, assign, readonly) int64_t folderId;
@property(nonatomic, assign, readonly) CGFloat position;
@property(nonatomic, assign, readonly) bookmarks::MenuItemType type;

+ (BookmarkPositionCache*)cacheForMenuItemFolderWithPosition:(CGFloat)position
                                                    folderId:(int64_t)folderId;

// This is not the designated initializer. It will return nil if there are
// problems with the decoding process.
- (instancetype)initWithCoder:(NSCoder*)coder;
@end

@interface BookmarkPositionCache (ExposedForTesting)
- (instancetype)initWithFolderId:(int64_t)folderId
                        position:(CGFloat)position
                            type:(bookmarks::MenuItemType)type;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_POSITION_CACHE_H_
