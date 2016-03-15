// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/wtf_array.h"

#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/wtf_serialization.h"
#include "mojo/public/cpp/bindings/tests/array_common_test.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using WTFArrayTest = testing::Test;

ARRAY_COMMON_TEST(WTFArray, NullAndEmpty)
ARRAY_COMMON_TEST(WTFArray, Basic)
ARRAY_COMMON_TEST(WTFArray, Bool)
ARRAY_COMMON_TEST(WTFArray, Handle)
ARRAY_COMMON_TEST(WTFArray, HandlesAreClosed)
ARRAY_COMMON_TEST(WTFArray, Clone)
ARRAY_COMMON_TEST(WTFArray, Serialization_ArrayOfPOD)
ARRAY_COMMON_TEST(WTFArray, Serialization_EmptyArrayOfPOD)
ARRAY_COMMON_TEST(WTFArray, Serialization_ArrayOfArrayOfPOD)
ARRAY_COMMON_TEST(WTFArray, Serialization_ArrayOfBool)
ARRAY_COMMON_TEST(WTFArray, Serialization_ArrayOfString)
ARRAY_COMMON_TEST(WTFArray, Resize_Copyable)
ARRAY_COMMON_TEST(WTFArray, Resize_MoveOnly)

TEST_F(WTFArrayTest, MoveFromAndToWTFVector_Copyable) {
  WTF::Vector<CopyableType> vec1(1);
  WTFArray<CopyableType> arr(std::move(vec1));
  ASSERT_EQ(1u, arr.size());
  ASSERT_FALSE(arr[0].copied());

  WTF::Vector<CopyableType> vec2(arr.PassStorage());
  ASSERT_EQ(1u, vec2.size());
  ASSERT_FALSE(vec2[0].copied());

  ASSERT_EQ(0u, arr.size());
  ASSERT_TRUE(arr.is_null());
}

TEST_F(WTFArrayTest, MoveFromAndToWTFVector_MoveOnly) {
  WTF::Vector<MoveOnlyType> vec1(1);
  WTFArray<MoveOnlyType> arr(std::move(vec1));

  ASSERT_EQ(1u, arr.size());

  WTF::Vector<MoveOnlyType> vec2(arr.PassStorage());
  ASSERT_EQ(1u, vec2.size());

  ASSERT_EQ(0u, arr.size());
  ASSERT_TRUE(arr.is_null());
}

}  // namespace
}  // namespace test
}  // namespace mojo
