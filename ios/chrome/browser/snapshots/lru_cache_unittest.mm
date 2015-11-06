// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/snapshots/lru_cache.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface LRUCacheTestDelegate : NSObject<LRUCacheDelegate>

@property(nonatomic, retain) id<NSObject> lastEvictedObject;
@property(nonatomic, assign) NSInteger evictedObjectsCount;

@end

@implementation LRUCacheTestDelegate

@synthesize lastEvictedObject = _lastEvictedObject;
@synthesize evictedObjectsCount = _evictedObjectsCount;

- (void)lruCacheWillEvictObject:(id<NSObject>)obj {
  self.lastEvictedObject = obj;
  ++_evictedObjectsCount;
}

@end

namespace {

TEST(LRUCacheTest, Basic) {
  base::scoped_nsobject<LRUCache> cache([[LRUCache alloc] initWithCacheSize:3]);
  base::scoped_nsobject<LRUCacheTestDelegate> delegate(
      [[LRUCacheTestDelegate alloc] init]);
  [cache setDelegate:delegate];

  base::scoped_nsobject<NSString> value1(
      [[NSString alloc] initWithString:@"Value 1"]);
  base::scoped_nsobject<NSString> value2(
      [[NSString alloc] initWithString:@"Value 2"]);
  base::scoped_nsobject<NSString> value3(
      [[NSString alloc] initWithString:@"Value 3"]);
  base::scoped_nsobject<NSString> value4(
      [[NSString alloc] initWithString:@"Value 4"]);

  EXPECT_TRUE([cache count] == 0);
  EXPECT_TRUE([cache isEmpty]);

  [cache setObject:value1 forKey:@"VALUE 1"];
  [cache setObject:value2 forKey:@"VALUE 2"];
  [cache setObject:value3 forKey:@"VALUE 3"];
  [cache setObject:value4 forKey:@"VALUE 4"];

  EXPECT_TRUE([cache count] == 3);
  EXPECT_TRUE([delegate evictedObjectsCount] == 1);
  EXPECT_TRUE([delegate lastEvictedObject] == value1.get());

  // Check LRU behaviour, the value least recently added value should have been
  // evicted.
  id value = [cache objectForKey:@"VALUE 1"];
  EXPECT_TRUE(value == nil);

  value = [cache objectForKey:@"VALUE 2"];
  EXPECT_TRUE(value == value2.get());

  // Removing a non existing key shouldn't do anything.
  [cache removeObjectForKey:@"XXX"];
  EXPECT_TRUE([cache count] == 3);

  [cache removeAllObjects];
  EXPECT_TRUE([cache isEmpty]);
}

}  // namespace
