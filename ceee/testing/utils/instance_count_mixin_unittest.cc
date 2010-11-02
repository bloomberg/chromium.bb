// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Implementation of instance count mixin class.
#include "ceee/testing/utils/instance_count_mixin.h"
#include "gtest/gtest.h"

namespace testing {

class TestMixin1 : public InstanceCountMixin<TestMixin1> {
 public:
};
class TestMixin2 : public InstanceCountMixin<TestMixin2> {
 public:
};

TEST(InstanceCountMixinTest, AllInstanceCounts) {
  ASSERT_EQ(0, TestMixin1::all_instance_count());
  ASSERT_EQ(0, TestMixin2::all_instance_count());

  {
    TestMixin1 one;

    ASSERT_EQ(1, TestMixin1::all_instance_count());
    ASSERT_EQ(1, TestMixin2::all_instance_count());

    TestMixin2 two;

    ASSERT_EQ(2, TestMixin1::all_instance_count());
    ASSERT_EQ(2, TestMixin2::all_instance_count());
  }

  ASSERT_EQ(0, TestMixin1::all_instance_count());
  ASSERT_EQ(0, TestMixin2::all_instance_count());
}

TEST(InstanceCountMixinTest, ClassInstanceCounts) {
  ASSERT_EQ(0, TestMixin1::instance_count());
  ASSERT_EQ(0, TestMixin2::instance_count());

  {
    TestMixin1 one;

    ASSERT_EQ(1, TestMixin1::instance_count());
    ASSERT_EQ(0, TestMixin2::instance_count());

    TestMixin2 two;

    ASSERT_EQ(1, TestMixin1::instance_count());
    ASSERT_EQ(1, TestMixin2::instance_count());
  }

  ASSERT_EQ(0, TestMixin1::instance_count());
  ASSERT_EQ(0, TestMixin2::instance_count());
}

TEST(InstanceCountMixinTest, Iteration) {
  ASSERT_TRUE(TestMixin1::begin() == TestMixin1::end());
  ASSERT_TRUE(TestMixin1::begin() == TestMixin2::begin());

  TestMixin1 one, two;
  TestMixin2 three, four;

  InstanceCountMixinBase::InstanceSet::const_iterator
      it(InstanceCountMixinBase::begin());
  InstanceCountMixinBase::InstanceSet::const_iterator
      end(InstanceCountMixinBase::end());
  size_t count = 0;

  for (; it != end; ++it) {
    ++count;
    ASSERT_TRUE(*it == &one || *it == &two || *it == &three || *it == &four);
  }

  ASSERT_EQ(4, count);
}

}  // namespace testing
