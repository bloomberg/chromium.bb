// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/xcallback_parameters.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
NSString* const kSourceAppIdKey = @"sourceAppId";
}

@implementation XCallbackParameters
@synthesize sourceAppId = _sourceAppId;

- (instancetype)initWithSourceAppId:(NSString*)sourceAppId {
  self = [super init];
  if (self) {
    _sourceAppId = [sourceAppId copy];
  }
  return self;
}

- (NSString*)description {
  return [NSString stringWithFormat:@"SourceApp: %@\n", _sourceAppId];
}

#pragma mark - NSCoding Methods

- (instancetype)initWithCoder:(NSCoder*)aDecoder {
  return
      [self initWithSourceAppId:[aDecoder decodeObjectForKey:kSourceAppIdKey]];
}

- (void)encodeWithCoder:(NSCoder*)aCoder {
  [aCoder encodeObject:_sourceAppId forKey:kSourceAppIdKey];
}

#pragma mark - NSCopying Methods

- (instancetype)copyWithZone:(NSZone*)zone {
  return [[[self class] allocWithZone:zone] initWithSourceAppId:_sourceAppId];
}

@end
