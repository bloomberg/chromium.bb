// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/array.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/tests/array_common_test.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using ArrayTest = testing::Test;

ARRAY_COMMON_TEST(Array, NullAndEmpty)
ARRAY_COMMON_TEST(Array, Basic)
ARRAY_COMMON_TEST(Array, Bool)
ARRAY_COMMON_TEST(Array, Handle)
ARRAY_COMMON_TEST(Array, HandlesAreClosed)
ARRAY_COMMON_TEST(Array, Clone)
ARRAY_COMMON_TEST(Array, Serialization_ArrayOfPOD)
ARRAY_COMMON_TEST(Array, Serialization_EmptyArrayOfPOD)
ARRAY_COMMON_TEST(Array, Serialization_ArrayOfArrayOfPOD)
ARRAY_COMMON_TEST(Array, Serialization_ArrayOfBool)
ARRAY_COMMON_TEST(Array, Serialization_ArrayOfString)
ARRAY_COMMON_TEST(Array, Resize_Copyable)
ARRAY_COMMON_TEST(Array, Resize_MoveOnly)

TEST_F(ArrayTest, PushBack_Copyable) {
  ASSERT_EQ(0u, CopyableType::num_instances());
  Array<CopyableType> array(2);
  array = nullptr;
  std::vector<CopyableType*> value_ptrs;
  size_t capacity = array.storage().capacity();
  for (size_t i = 0; i < capacity; i++) {
    CopyableType value;
    value_ptrs.push_back(value.ptr());
    array.push_back(value);
    ASSERT_EQ(i + 1, array.size());
    ASSERT_EQ(i + 1, value_ptrs.size());
    EXPECT_EQ(array.size() + 1, CopyableType::num_instances());
    EXPECT_TRUE(array[i].copied());
    EXPECT_EQ(value_ptrs[i], array[i].ptr());
    array[i].ResetCopied();
    EXPECT_TRUE(array);
  }
  {
    CopyableType value;
    value_ptrs.push_back(value.ptr());
    array.push_back(value);
    EXPECT_EQ(array.size() + 1, CopyableType::num_instances());
  }
  ASSERT_EQ(capacity + 1, array.size());
  EXPECT_EQ(array.size(), CopyableType::num_instances());

  for (size_t i = 0; i < array.size(); i++) {
    EXPECT_TRUE(array[i].copied());
    EXPECT_EQ(value_ptrs[i], array[i].ptr());
  }
  array = nullptr;
  EXPECT_EQ(0u, CopyableType::num_instances());
}

TEST_F(ArrayTest, PushBack_MoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  Array<MoveOnlyType> array(2);
  array = nullptr;
  std::vector<MoveOnlyType*> value_ptrs;
  size_t capacity = array.storage().capacity();
  for (size_t i = 0; i < capacity; i++) {
    MoveOnlyType value;
    value_ptrs.push_back(value.ptr());
    array.push_back(std::move(value));
    ASSERT_EQ(i + 1, array.size());
    ASSERT_EQ(i + 1, value_ptrs.size());
    EXPECT_EQ(array.size() + 1, MoveOnlyType::num_instances());
    EXPECT_TRUE(array[i].moved());
    EXPECT_EQ(value_ptrs[i], array[i].ptr());
    array[i].ResetMoved();
    EXPECT_TRUE(array);
  }
  {
    MoveOnlyType value;
    value_ptrs.push_back(value.ptr());
    array.push_back(std::move(value));
    EXPECT_EQ(array.size() + 1, MoveOnlyType::num_instances());
  }
  ASSERT_EQ(capacity + 1, array.size());
  EXPECT_EQ(array.size(), MoveOnlyType::num_instances());

  for (size_t i = 0; i < array.size(); i++) {
    EXPECT_TRUE(array[i].moved());
    EXPECT_EQ(value_ptrs[i], array[i].ptr());
  }
  array = nullptr;
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
}

TEST_F(ArrayTest, MoveFromAndToSTLVector_Copyable) {
  std::vector<CopyableType> vec1(1);
  Array<CopyableType> arr(std::move(vec1));
  ASSERT_EQ(1u, arr.size());
  ASSERT_FALSE(arr[0].copied());

  std::vector<CopyableType> vec2(arr.PassStorage());
  ASSERT_EQ(1u, vec2.size());
  ASSERT_FALSE(vec2[0].copied());

  ASSERT_EQ(0u, arr.size());
  ASSERT_TRUE(arr.is_null());
}

TEST_F(ArrayTest, MoveFromAndToSTLVector_MoveOnly) {
  std::vector<MoveOnlyType> vec1(1);
  Array<MoveOnlyType> arr(std::move(vec1));

  ASSERT_EQ(1u, arr.size());

  std::vector<MoveOnlyType> vec2(arr.PassStorage());
  ASSERT_EQ(1u, vec2.size());

  ASSERT_EQ(0u, arr.size());
  ASSERT_TRUE(arr.is_null());
}

}  // namespace
}  // namespace test
}  // namespace mojo
