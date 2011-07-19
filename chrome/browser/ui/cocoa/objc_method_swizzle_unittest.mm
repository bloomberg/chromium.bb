// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/objc_method_swizzle.h"

#include "base/memory/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

@interface ObjcMethodSwizzleTest : NSObject
- (id)self;

- (NSInteger)one;
- (NSInteger)two;
@end

@implementation ObjcMethodSwizzleTest : NSObject
- (id)self {
  return [super self];
}

- (NSInteger)one {
  return 1;
}
- (NSInteger)two {
  return 2;
}
@end

@interface ObjcMethodSwizzleTest (ObjcMethodSwizzleTestCategory)
- (NSUInteger)hash;
@end

@implementation ObjcMethodSwizzleTest (ObjcMethodSwizzleTestCategory)
- (NSUInteger)hash {
  return [super hash];
}
@end

namespace ObjcEvilDoers {

TEST(ObjcMethodSwizzleTest, GetImplementedInstanceMethod) {
  EXPECT_EQ(class_getInstanceMethod([NSObject class], @selector(dealloc)),
            GetImplementedInstanceMethod([NSObject class], @selector(dealloc)));
  EXPECT_EQ(class_getInstanceMethod([NSObject class], @selector(self)),
            GetImplementedInstanceMethod([NSObject class], @selector(self)));
  EXPECT_EQ(class_getInstanceMethod([NSObject class], @selector(hash)),
            GetImplementedInstanceMethod([NSObject class], @selector(hash)));

  Class testClass = [ObjcMethodSwizzleTest class];
  EXPECT_EQ(class_getInstanceMethod(testClass, @selector(self)),
            GetImplementedInstanceMethod(testClass, @selector(self)));
  EXPECT_NE(class_getInstanceMethod([NSObject class], @selector(self)),
            class_getInstanceMethod(testClass, @selector(self)));

  EXPECT_TRUE(class_getInstanceMethod(testClass, @selector(dealloc)));
  EXPECT_FALSE(GetImplementedInstanceMethod(testClass, @selector(dealloc)));
}

TEST(ObjcMethodSwizzleTest, SwizzleImplementedInstanceMethods) {
  scoped_nsobject<ObjcMethodSwizzleTest> object(
      [[ObjcMethodSwizzleTest alloc] init]);
  EXPECT_EQ([object one], 1);
  EXPECT_EQ([object two], 2);

  Class testClass = [object class];
  SwizzleImplementedInstanceMethods(testClass, @selector(one), @selector(two));
  EXPECT_EQ([object one], 2);
  EXPECT_EQ([object two], 1);

  SwizzleImplementedInstanceMethods(testClass, @selector(one), @selector(two));
  EXPECT_EQ([object one], 1);
  EXPECT_EQ([object two], 2);
}

}  // namespace ObjcEvilDoers
