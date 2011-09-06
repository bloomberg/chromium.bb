// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#include <dlfcn.h>

#include "base/logging.h"
#import "base/memory/scoped_nsobject.h"
#import "chrome/common/mac/objc_zombie.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {

// Dynamically look up |objc_setAssociatedObject()|, which isn't
// available until the 10.6 SDK.

typedef void objc_setAssociatedObjectFn(id object, void *key, id value,
                                        int policy);
objc_setAssociatedObjectFn* LookupSetAssociatedObjectFn() {
  return reinterpret_cast<objc_setAssociatedObjectFn*>(
      dlsym(RTLD_DEFAULT, "objc_setAssociatedObject"));
}

}  // namespace

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

@interface ZombieAssociatedObjectTest : NSObject
+ (BOOL)supportsAssociatedObjects;
- (id)initWithAssociatedObject:(id)anObject;
@end

@implementation ZombieAssociatedObjectTest

+ (BOOL)supportsAssociatedObjects {
  if (LookupSetAssociatedObjectFn())
    return YES;
  return NO;
}

- (id)initWithAssociatedObject:(id)anObject {
  self = [super init];
  if (self) {
    objc_setAssociatedObjectFn* fn = LookupSetAssociatedObjectFn();
    if (fn) {
      // Cribbed from 10.6 <objc/runtime.h>.
      static const int kObjcAssociationRetain = 01401;

      // The address of the variable itself is the unique key, the
      // contents don't matter.
      static char kAssociatedObjectKey = 'x';

      (*fn)(self, &kAssociatedObjectKey, anObject, kObjcAssociationRetain);
    }
  }
  return self;
}

@end

namespace {

// Verify that the C++ destructors run when the last reference to the
// object is released.
// NOTE(shess): To test the negative, comment out the |g_objectDestruct()|
// call in |ZombieDealloc()|.
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

// Verify that the associated objects are released when the object is
// released.
// NOTE(shess): To test the negative, hardcode |g_objectDestruct| to
// the 10.5 version in |ZombieInit()|, and run this test on 10.6.
TEST(ObjcZombieTest, AssociatedObjectsReleased) {
  if (![ZombieAssociatedObjectTest supportsAssociatedObjects]) {
    LOG(ERROR)
        << "ObjcZombieTest.AssociatedObjectsReleased not supported on 10.5";
    return;
  }

  scoped_nsobject<id> anObject([[NSObject alloc] init]);
  EXPECT_EQ(1u, [anObject retainCount]);

  ASSERT_TRUE(ObjcEvilDoers::ZombieEnable(YES, 100));

  scoped_nsobject<ZombieAssociatedObjectTest> soonInfected(
      [[ZombieAssociatedObjectTest alloc] initWithAssociatedObject:anObject]);
  EXPECT_EQ(2u, [anObject retainCount]);

  // When |soonInfected| becomes a zombie, the associated object
  // should be released.
  soonInfected.reset();
  EXPECT_EQ(1u, [anObject retainCount]);

  // The local reference should remain (associated objects not re-released).
  ObjcEvilDoers::ZombieDisable();
  EXPECT_EQ(1u, [anObject retainCount]);
}

}  // namespace
