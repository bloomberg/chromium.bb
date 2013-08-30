// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/scoped_observer_with_duplicated_sources.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

class TestObserver {
 public:
  TestObserver() {}
  ~TestObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

class TestSource {
 public:
  TestSource() : observer_count_(0) {}
  ~TestSource() {}

  void AddObserver(TestObserver* observer) {
    observer_count_++;
  }
  void RemoveObserver(TestObserver* observer) {
    observer_count_--;
  }

  int GetObserverCount() {
    return observer_count_;
  }

 private:
  int observer_count_;

  DISALLOW_COPY_AND_ASSIGN(TestSource);
};

TEST(ScopedObserverWithDuplicatedSourcesTest, DuplicatedSource) {
  TestObserver observer;
  TestSource source1;
  TestSource source2;

  ScopedObserverWithDuplicatedSources<TestSource, TestObserver>
      observers(&observer);
  EXPECT_EQ(0, source1.GetObserverCount());
  EXPECT_FALSE(observers.IsObserving(&source1));
  EXPECT_EQ(0, source2.GetObserverCount());
  EXPECT_FALSE(observers.IsObserving(&source2));

  // Add |source1|.
  observers.Add(&source1);
  EXPECT_EQ(1, source1.GetObserverCount());
  EXPECT_TRUE(observers.IsObserving(&source1));
  // AddObserver of TestSource is called only once.
  observers.Add(&source1);
  EXPECT_EQ(1, source1.GetObserverCount());
  EXPECT_TRUE(observers.IsObserving(&source1));

  // Add |source2|.
  observers.Add(&source2);
  EXPECT_EQ(1, source2.GetObserverCount());
  EXPECT_TRUE(observers.IsObserving(&source2));

  // Remove |source1|.
  observers.Remove(&source1);
  EXPECT_EQ(1, source1.GetObserverCount());
  EXPECT_TRUE(observers.IsObserving(&source1));

  // Remove |source2|.
  observers.Remove(&source2);
  EXPECT_EQ(0, source2.GetObserverCount());
  EXPECT_FALSE(observers.IsObserving(&source2));

  // Remove |source1| again.
  observers.Remove(&source1);
  // In this time, |observer| is removed from |source1|.
  EXPECT_EQ(0, source1.GetObserverCount());
  EXPECT_FALSE(observers.IsObserving(&source1));
}
