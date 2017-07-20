// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_test_back_forward_list.h"

#import <WebKit/WebKit.h>

#include "third_party/ocmock/OCMock/OCMock.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface CRWTestBackForwardList (PrivateMethods)
- (WKBackForwardListItem*)mockItemWithURLString:(NSString*)URL;
- (NSArray*)mockSublistWithURLArray:(NSArray<NSString*>*)URLs;
@end

@implementation CRWTestBackForwardList

@synthesize backList;
@synthesize forwardList;
@synthesize currentItem;

- (WKBackForwardListItem*)itemAtIndex:(NSInteger)index {
  if (index == 0) {
    return self.currentItem;
  } else if (index > 0 && self.forwardList.count) {
    return self.forwardList[index - 1];
  } else if (self.backList.count) {
    return self.backList[self.backList.count + index];
  }
  return nil;
}

- (void)setCurrentURL:(NSString*)currentItemURL {
  [self setCurrentURL:currentItemURL backListURLs:nil forwardListURLs:nil];
}

- (void)setCurrentURL:(NSString*)currentItemURL
         backListURLs:(nullable NSArray<NSString*>*)backListURLs
      forwardListURLs:(nullable NSArray<NSString*>*)forwardListURLs {
  self.currentItem = [self mockItemWithURLString:currentItemURL];
  self.backList = [self mockSublistWithURLArray:backListURLs];
  self.forwardList = [self mockSublistWithURLArray:forwardListURLs];
}

- (WKBackForwardListItem*)mockItemWithURLString:(NSString*)URL {
  id mock = OCMClassMock([WKBackForwardListItem class]);
  OCMStub([mock URL]).andReturn([NSURL URLWithString:URL]);
  return mock;
}

- (NSArray*)mockSublistWithURLArray:(NSArray<NSString*>*)URLs {
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:URLs.count];
  for (NSString* URL : URLs) {
    [array addObject:[self mockItemWithURLString:URL]];
  }
  return [NSArray arrayWithArray:array];
}

@end
