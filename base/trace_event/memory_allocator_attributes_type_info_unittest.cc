// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_allocator_attributes_type_info.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace trace_event {

TEST(MemoryAllocatorAttributesTypeInfoTest, BasicTest) {
  MemoryAllocatorAttributesTypeInfo attrs;
  EXPECT_EQ("", attrs.Get("non_existing_alloc", "non_existing_attr"));

  attrs.Set("alloc", "attr", "type");
  EXPECT_TRUE(attrs.Exists("alloc", "attr"));
  EXPECT_FALSE(attrs.Exists("alloc", "foo"));
  EXPECT_FALSE(attrs.Exists("foo", "attr"));
  EXPECT_EQ("type", attrs.Get("alloc", "attr"));

  attrs.Set("alloc2", "attr", "type2");
  EXPECT_TRUE(attrs.Exists("alloc2", "attr"));
  EXPECT_FALSE(attrs.Exists("alloc2", "foo"));
  EXPECT_EQ("type", attrs.Get("alloc", "attr"));
  EXPECT_EQ("type2", attrs.Get("alloc2", "attr"));

  MemoryAllocatorAttributesTypeInfo other_attrs;
  other_attrs.Set("other_alloc", "other_attr", "other_type");
  other_attrs.Set("other_alloc", "attr", "other_type2");
  other_attrs.Set("other_alloc_2", "other_attr", "other_type");
  other_attrs.Set("other_alloc_2", "attr", "other_type3");

  // Check the merging logic.
  attrs.Update(other_attrs);
  EXPECT_EQ("other_type", attrs.Get("other_alloc", "other_attr"));
  EXPECT_EQ("other_type2", attrs.Get("other_alloc", "attr"));
  EXPECT_EQ("other_type", attrs.Get("other_alloc_2", "other_attr"));
  EXPECT_EQ("other_type3", attrs.Get("other_alloc_2", "attr"));
  EXPECT_EQ("type", attrs.Get("alloc", "attr"));
  EXPECT_EQ("type2", attrs.Get("alloc2", "attr"));
  EXPECT_FALSE(other_attrs.Exists("alloc", "attr"));
  EXPECT_FALSE(other_attrs.Exists("alloc2", "attr"));
}

// DEATH tests are not supported in Android / iOS.
#if !defined(NDEBUG) && !defined(OS_ANDROID) && !defined(OS_IOS)
TEST(MemoryAllocatorAttributesTypeInfoTest, DuplicatesDeathTest) {
  MemoryAllocatorAttributesTypeInfo attrs;
  attrs.Set("alloc", "attr", "type");
  MemoryAllocatorAttributesTypeInfo conflicting_attrs;
  conflicting_attrs.Set("alloc", "attr", "type2");
  ASSERT_DEATH(attrs.Set("alloc", "attr", "other_type"), "");
  ASSERT_DEATH(attrs.Update(conflicting_attrs), "");
}
#endif

}  // namespace trace_event
}  // namespace base
