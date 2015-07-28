// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/weak_nsobject_counter.h"

#import <Foundation/Foundation.h>

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Tests that a WeakNSObjectCounter correctly maintains a weak reference and
// calculates the size correctly when many objects are added.
TEST(WeakNSObjectCounter, ManyObjects) {
  web::WeakNSObjectCounter object_counter;
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  base::scoped_nsobject<NSObject> p2([[NSObject alloc] init]);
  base::scoped_nsobject<NSObject> p3([[NSObject alloc] init]);
  object_counter.Insert(p1);
  EXPECT_EQ(1U, object_counter.Size());
  object_counter.Insert(p2);
  EXPECT_EQ(2U, object_counter.Size());
  object_counter.Insert(p3);
  EXPECT_EQ(3U, object_counter.Size());
  // Make sure p3 is correctly removed from the object counter.
  p3.reset();
  EXPECT_FALSE(p3);
  EXPECT_EQ(2U, object_counter.Size());
  // Make sure p2 is correctly removed from the object counter.
  p2.reset();
  EXPECT_EQ(1U, object_counter.Size());
  // Make sure p1 is correctly removed from the object counter.
  p1.reset();
  EXPECT_FALSE(p1);
  EXPECT_EQ(0U, object_counter.Size());
}

// Tests that WeakNSObjectCounter correctly works when the object outlives the
// WeakNSObjectCounter that is observing it.
TEST(WeakNSObjectCounter, ObjectOutlivesWeakNSObjectCounter) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  {
    web::WeakNSObjectCounter object_counter;
    object_counter.Insert(p1);
    EXPECT_EQ(1U, object_counter.Size());
  }
  p1.reset();
  EXPECT_FALSE(p1);
}

// Tests that WeakNSObjectCounter does not add the same object twice.
TEST(WeakNSObjectCounter, SameObject) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  base::WeakNSObject<NSObject> p2(p1);
  // Make sure they are the same object.
  ASSERT_EQ(p1, p2);
  web::WeakNSObjectCounter object_counter;
  object_counter.Insert(p1);
  EXPECT_EQ(1U, object_counter.Size());
  object_counter.Insert(p2);
  EXPECT_EQ(1U, object_counter.Size());
}

// Tests that WeakNSObjectCounters keep track of the same object correctly.
TEST(WeakNSObjectCounter, SameObjectTwoCounters) {
  base::scoped_nsobject<NSObject> object([[NSObject alloc] init]);
  web::WeakNSObjectCounter object_counter_1;
  web::WeakNSObjectCounter object_counter_2;
  object_counter_1.Insert(object);
  EXPECT_EQ(1U, object_counter_1.Size());

  object_counter_2.Insert(object);
  EXPECT_EQ(1U, object_counter_1.Size());
  EXPECT_EQ(1U, object_counter_2.Size());

  object.reset();
  EXPECT_EQ(0U, object_counter_1.Size());
  EXPECT_EQ(0U, object_counter_2.Size());
}

// Tests that WeakNSObjectCounter correctly maintains a reference to the object
// and reduces its size only when the last reference to the object goes away.
TEST(WeakNSObjectCounter, LastReference) {
  web::WeakNSObjectCounter object_counter;
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  object_counter.Insert(p1);
  EXPECT_EQ(1U, object_counter.Size());
  base::scoped_nsobject<NSObject> p2(p1);
  base::scoped_nsobject<NSObject> p3(p1);
  p1.reset();
  EXPECT_EQ(1U, object_counter.Size());
  p2.reset();
  EXPECT_EQ(1U, object_counter.Size());
  // Last reference goes away.
  p3.reset();
  EXPECT_FALSE(object_counter.Size());
}

}  // namespace
