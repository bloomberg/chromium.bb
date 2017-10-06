// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The current version of the cached position. This number should be incremented
// each time the NSCoding implementation changes.
const int kVersion = 1;

NSString* kBookmarkFolderIdKey = @"BookmarkFolderIdKey";
NSString* kPositionKey = @"PositionKey";
NSString* kVersionKey = @"VersionKey";
}  // namespace

@interface BookmarkPathCache ()

// This is the designated initializer. It does not perform any validation.
- (instancetype)initWithBookmarkFolder:(int64_t)folderId
                              position:(CGFloat)position
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

@implementation BookmarkPathCache
@synthesize position = _position;
@synthesize folderId = _folderId;

#pragma mark - Public Constructors

+ (BookmarkPathCache*)cacheForBookmarkFolder:(int64_t)folderId
                                    position:(CGFloat)position {
  return [[BookmarkPathCache alloc] initWithBookmarkFolder:folderId
                                                  position:position];
}

#pragma mark - Designated Initializer

- (instancetype)initWithBookmarkFolder:(int64_t)folderId
                              position:(CGFloat)position {
  self = [super init];
  if (self) {
    _folderId = folderId;
    _position = position;
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
  if (![object isKindOfClass:[BookmarkPathCache class]])
    return NO;
  BookmarkPathCache* other = static_cast<BookmarkPathCache*>(object);
  if (fabs(self.position - other.position) > 0.01)
    return NO;
  if (self.folderId != other.folderId)
    return NO;

  return YES;
}

- (NSUInteger)hash {
  return static_cast<NSUInteger>(self.folderId) ^
         static_cast<NSUInteger>(self.position);
}

#pragma mark - NSCoding

- (instancetype)initWithCoder:(NSCoder*)coder {
  int version = [coder decodeIntForKey:kVersionKey];
  if (version != kVersion) {
    return nil;
  }

  return [self
      initWithBookmarkFolder:[coder decodeInt64ForKey:kBookmarkFolderIdKey]
                    position:[coder decodeFloatForKey:kPositionKey]];
}

- (void)encodeWithCoder:(NSCoder*)coder {
  [coder encodeInt:kVersion forKey:kVersionKey];
  [coder encodeFloat:self.position forKey:kPositionKey];
  [coder encodeInt64:self.folderId forKey:kBookmarkFolderIdKey];
}

@end
