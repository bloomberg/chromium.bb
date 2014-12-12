// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/ios/weak_nsobject.h"
#include "base/mac/scoped_nsobject.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WeakNSObject;

namespace {

TEST(WeakNSObjectTest, WeakNSObject) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  p1.reset();
  EXPECT_FALSE(w1);
}

TEST(WeakNSObjectTest, MultipleWeakNSObject) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2(w1);
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  EXPECT_TRUE(w1.get() == w2.get());
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

TEST(WeakNSObjectTest, WeakNSObjectDies) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  {
    WeakNSObject<NSObject> w1(p1);
    EXPECT_TRUE(w1);
  }
}

TEST(WeakNSObjectTest, WeakNSObjectReset) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  w1.reset();
  EXPECT_FALSE(w1);
  EXPECT_TRUE(p1);
  EXPECT_TRUE([p1 description]);
}

TEST(WeakNSObjectTest, WeakNSObjectResetWithObject) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  base::scoped_nsobject<NSObject> p2([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  EXPECT_TRUE(w1);
  w1.reset(p2);
  EXPECT_TRUE(w1);
  EXPECT_TRUE([p1 description]);
  EXPECT_TRUE([p2 description]);
}

TEST(WeakNSObjectTest, WeakNSObjectEmpty) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1;
  EXPECT_FALSE(w1);
  w1.reset(p1);
  EXPECT_TRUE(w1);
  p1.reset();
  EXPECT_FALSE(w1);
}

TEST(WeakNSObjectTest, WeakNSObjectCopy) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2(w1);
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

TEST(WeakNSObjectTest, WeakNSObjectAssignment) {
  base::scoped_nsobject<NSObject> p1([[NSObject alloc] init]);
  WeakNSObject<NSObject> w1(p1);
  WeakNSObject<NSObject> w2;
  EXPECT_FALSE(w2);
  w2 = w1;
  EXPECT_TRUE(w1);
  EXPECT_TRUE(w2);
  p1.reset();
  EXPECT_FALSE(w1);
  EXPECT_FALSE(w2);
}

}  // namespace
