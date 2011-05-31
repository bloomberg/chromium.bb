// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/objc_zombie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

@interface ZombieCxxDestructTest : NSObject
{
  scoped_nsobject<id> aRef_;
}
- (id)initWith:(id)anObject;
@end

@implementation ZombieCxxDestructTest
- (id)initWith:(id)anObject {
  self = [super init];
  if (self) {
    aRef_.reset([anObject retain]);
  }
  return self;
}
@end

namespace {

// Verify that the C++ destructors run when the last reference to the
// object is released.  Unfortunately, testing the negative requires
// commenting out the |object_cxxDestruct()| call in
// |ZombieDealloc()|.

TEST(ObjcZombieTest, CxxDestructors) {
  scoped_nsobject<id> anObject([[NSObject alloc] init]);
  EXPECT_EQ(1u, [anObject retainCount]);

  ASSERT_TRUE(ObjcEvilDoers::ZombieEnable(YES, 100));

  scoped_nsobject<ZombieCxxDestructTest> soonInfected(
      [[ZombieCxxDestructTest alloc] initWith:anObject]);
  EXPECT_EQ(2u, [anObject retainCount]);

  // When |soonInfected| becomes a zombie, the C++ destructors should
  // run and release a reference to |anObject|.
  soonInfected.reset();
  EXPECT_EQ(1u, [anObject retainCount]);

  // The local reference should remain (C++ destructors aren't re-run).
  ObjcEvilDoers::ZombieDisable();
  EXPECT_EQ(1u, [anObject retainCount]);
}

}  // namespace
