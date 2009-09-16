// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/leak_tracker.h"
#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class ClassA {
 private:
  base::LeakTracker<ClassA> leak_tracker_;
};

class ClassB {
 private:
  base::LeakTracker<ClassB> leak_tracker_;
};

#ifndef ENABLE_LEAK_TRACKER

// If leak tracking is disabled, we should do nothing.
TEST(LeakTrackerTest, NotEnabled) {
  EXPECT_EQ(-1, base::LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(-1, base::LeakTracker<ClassB>::NumLiveInstances());

  // Use scoped_ptr so compiler doesn't complain about unused variables.
  scoped_ptr<ClassA> a1(new ClassA);
  scoped_ptr<ClassB> b1(new ClassB);
  scoped_ptr<ClassB> b2(new ClassB);

  EXPECT_EQ(-1, base::LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(-1, base::LeakTracker<ClassB>::NumLiveInstances());
}

#else

TEST(LeakTrackerTest, Basic) {
  {
    ClassA a1;

    EXPECT_EQ(1, base::LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(0, base::LeakTracker<ClassB>::NumLiveInstances());

    ClassB b1;
    ClassB b2;

    EXPECT_EQ(1, base::LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, base::LeakTracker<ClassB>::NumLiveInstances());

    scoped_ptr<ClassA> a2(new ClassA);

    EXPECT_EQ(2, base::LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, base::LeakTracker<ClassB>::NumLiveInstances());

    a2.reset();

    EXPECT_EQ(1, base::LeakTracker<ClassA>::NumLiveInstances());
    EXPECT_EQ(2, base::LeakTracker<ClassB>::NumLiveInstances());
  }

  EXPECT_EQ(0, base::LeakTracker<ClassA>::NumLiveInstances());
  EXPECT_EQ(0, base::LeakTracker<ClassB>::NumLiveInstances());
}

// Try some orderings of create/remove to hit different cases in the linked-list
// assembly.
TEST(LeakTrackerTest, LinkedList) {
  EXPECT_EQ(0, base::LeakTracker<ClassB>::NumLiveInstances());

  scoped_ptr<ClassA> a1(new ClassA);
  scoped_ptr<ClassA> a2(new ClassA);
  scoped_ptr<ClassA> a3(new ClassA);
  scoped_ptr<ClassA> a4(new ClassA);

  EXPECT_EQ(4, base::LeakTracker<ClassA>::NumLiveInstances());

  // Remove the head of the list (a1).
  a1.reset();
  EXPECT_EQ(3, base::LeakTracker<ClassA>::NumLiveInstances());

  // Remove the tail of the list (a4).
  a4.reset();
  EXPECT_EQ(2, base::LeakTracker<ClassA>::NumLiveInstances());

  // Append to the new tail of the list (a3).
  scoped_ptr<ClassA> a5(new ClassA);
  EXPECT_EQ(3, base::LeakTracker<ClassA>::NumLiveInstances());

  a2.reset();
  a3.reset();

  EXPECT_EQ(1, base::LeakTracker<ClassA>::NumLiveInstances());

  a5.reset();
  EXPECT_EQ(0, base::LeakTracker<ClassA>::NumLiveInstances());
}

TEST(LeakTrackerTest, NoOpCheckForLeaks) {
  // There are no live instances of ClassA, so this should do nothing.
  base::LeakTracker<ClassA>::CheckForLeaks();
}

#endif  // ENABLE_LEAK_TRACKER

}  // namespace
