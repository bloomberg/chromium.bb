// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/wtf_map.h"

#include "mojo/public/cpp/bindings/lib/wtf_serialization.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "mojo/public/cpp/bindings/tests/map_common_test.h"
#include "mojo/public/cpp/bindings/wtf_array.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/wtf/text/WTFString.h"

namespace mojo {
namespace test {

namespace {

using WTFMapTest = testing::Test;

MAP_COMMON_TEST(WTFMap, NullAndEmpty)
MAP_COMMON_TEST(WTFMap, InsertWorks)
MAP_COMMON_TEST(WTFMap, TestIndexOperator)
MAP_COMMON_TEST(WTFMap, TestIndexOperatorAsRValue)
MAP_COMMON_TEST(WTFMap, TestIndexOperatorMoveOnly)
MAP_COMMON_TEST(WTFMap, MapArrayClone)
MAP_COMMON_TEST(WTFMap, ArrayOfMap)

TEST_F(WTFMapTest, MoveFromAndToWTFHashMap_Copyable) {
  WTF::HashMap<int32_t, CopyableType> map1;
  map1.add(123, CopyableType());
  map1.find(123)->value.ResetCopied();
  ASSERT_FALSE(map1.find(123)->value.copied());

  WTFMap<int32_t, CopyableType> mojo_map(std::move(map1));
  ASSERT_EQ(1u, mojo_map.size());
  ASSERT_NE(mojo_map.end(), mojo_map.find(123));
  ASSERT_FALSE(mojo_map[123].copied());

  WTF::HashMap<int32_t, CopyableType> map2(mojo_map.PassStorage());
  ASSERT_EQ(1u, map2.size());
  ASSERT_NE(map2.end(), map2.find(123));
  ASSERT_FALSE(map2.find(123)->value.copied());

  ASSERT_EQ(0u, mojo_map.size());
  ASSERT_TRUE(mojo_map.is_null());
}

TEST_F(WTFMapTest, MoveFromAndToWTFHashMap_MoveOnly) {
  WTF::HashMap<int32_t, MoveOnlyType> map1;
  map1.add(123, MoveOnlyType());

  WTFMap<int32_t, MoveOnlyType> mojo_map(std::move(map1));
  ASSERT_EQ(1u, mojo_map.size());
  ASSERT_NE(mojo_map.end(), mojo_map.find(123));

  WTF::HashMap<int32_t, MoveOnlyType> map2(mojo_map.PassStorage());
  ASSERT_EQ(1u, map2.size());
  ASSERT_NE(map2.end(), map2.find(123));

  ASSERT_EQ(0u, mojo_map.size());
  ASSERT_TRUE(mojo_map.is_null());
}

}  // namespace
}  // namespace test
}  // namespace mojo
