// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_
#define IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Caches the user's position in the bookmark hierarchy navigator, as well as
// scroll position.
@interface BookmarkPathCache : NSObject<NSCoding>

@property(nonatomic, assign, readonly) CGFloat position;
@property(nonatomic, assign, readonly) int64_t folderId;

+ (BookmarkPathCache*)cacheForBookmarkFolder:(int64_t)folderId
                                    position:(CGFloat)position;

// This is not the designated initializer. It will return nil if there are
// problems with the decoding process.
- (instancetype)initWithCoder:(NSCoder*)coder;
@end

@interface BookmarkPathCache (ExposedForTesting)
- (instancetype)cacheForBookmarkFolder:(int64_t)folderId
                              position:(CGFloat)position;
@end

#endif  // IOS_CHROME_BROWSER_UI_BOOKMARKS_BOOKMARK_PATH_CACHE_H_
