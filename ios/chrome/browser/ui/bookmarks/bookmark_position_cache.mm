// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_position_cache.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/mac/objc_property_releaser.h"

namespace {
// The current version of the cached position. This number should be incremented
// each time the NSCoding implementation changes.
const int kVersion = 2;

// The value 3 was used for items corresponding to managed bookmarks and
// was removed during the transition from UIWebView to WKWebView.
const int kMenuItemManaged = 3;

NSString* kFolderKey = @"FolderKey";
NSString* kTypeKey = @"TypeKey";
NSString* kPositionKey = @"PositionKey";
NSString* kVersionKey = @"VersionKey";
}  // namespace

@interface BookmarkPositionCache ()

// This is the designated initializer. It does not perform any validation.
- (instancetype)initWithFolderId:(int64_t)folderId
                        position:(CGFloat)position
                            type:(bookmarks::MenuItemType)type
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation BookmarkPositionCache
@synthesize folderId = _folderId;
@synthesize position = _position;
@synthesize type = _type;

#pragma mark - Public Constructors

+ (BookmarkPositionCache*)cacheForMenuItemFolderWithPosition:(CGFloat)position
                                                    folderId:(int64_t)folderId {
  return [[[BookmarkPositionCache alloc]
      initWithFolderId:folderId
              position:position
                  type:bookmarks::MenuItemFolder] autorelease];
}

#pragma mark - Designated Initializer

- (instancetype)initWithFolderId:(int64_t)folderId
                        position:(CGFloat)position
                            type:(bookmarks::MenuItemType)type {
  self = [super init];
  if (self) {
    _folderId = folderId;
    _position = position;
    _type = type;
  }
  return self;
}

#pragma mark - Superclass Overrides

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (BOOL)isEqual:(id)object {
  if (self == object)
    return YES;
  if (![object isKindOfClass:[BookmarkPositionCache class]])
    return NO;
  BookmarkPositionCache* other = static_cast<BookmarkPositionCache*>(object);
  if (self.type != other.type)
    return NO;
  if (fabs(self.position - other.position) > 0.01)
    return NO;
  switch (self.type) {
    case bookmarks::MenuItemDivider:
    case bookmarks::MenuItemSectionHeader:
      return YES;
    case bookmarks::MenuItemFolder:
      return self.folderId == other.folderId;
  }
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.type) ^
         static_cast<NSUInteger>(self.folderId);
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)coder {
  int version = [coder decodeIntForKey:kVersionKey];
  int typeInt = [coder decodeIntForKey:kTypeKey];

  if (version != kVersion) {
    [self release];
    return nil;
  }

  if (!bookmarks::NumberIsValidMenuItemType(typeInt)) {
    [self release];
    return nil;
  }

  if (typeInt == kMenuItemManaged) {
    [self release];
    return nil;
  }

  bookmarks::MenuItemType type = static_cast<bookmarks::MenuItemType>(typeInt);
  return [self initWithFolderId:[coder decodeInt64ForKey:kFolderKey]
                       position:[coder decodeFloatForKey:kPositionKey]
                           type:type];
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeInt:kVersion forKey:kVersionKey];
  [coder encodeInt:static_cast<int>(self.type) forKey:kTypeKey];
  [coder encodeFloat:self.position forKey:kPositionKey];
  [coder encodeInt64:self.folderId forKey:kFolderKey];
}

@end
