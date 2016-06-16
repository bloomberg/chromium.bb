// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/map.h"

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "mojo/public/cpp/bindings/tests/map_common_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

namespace {

using MapTest = testing::Test;

MAP_COMMON_TEST(Map, NullAndEmpty)
MAP_COMMON_TEST(Map, InsertWorks)
MAP_COMMON_TEST(Map, TestIndexOperator)
MAP_COMMON_TEST(Map, TestIndexOperatorAsRValue)
MAP_COMMON_TEST(Map, TestIndexOperatorMoveOnly)
MAP_COMMON_TEST(Map, MapArrayClone)
MAP_COMMON_TEST(Map, ArrayOfMap)

TEST_F(MapTest, ConstructedFromArray) {
  Array<String> keys(kStringIntDataSize);
  Array<int> values(kStringIntDataSize);
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    keys[i] = kStringIntData[i].string_data;
    values[i] = kStringIntData[i].int_data;
  }

  Map<String, int> map(std::move(keys), std::move(values));

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    EXPECT_EQ(kStringIntData[i].int_data,
              map.at(mojo::String(kStringIntData[i].string_data)));
  }
}

TEST_F(MapTest, DecomposeMapTo) {
  Array<String> keys(kStringIntDataSize);
  Array<int> values(kStringIntDataSize);
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    keys[i] = kStringIntData[i].string_data;
    values[i] = kStringIntData[i].int_data;
  }

  Map<String, int> map(std::move(keys), std::move(values));
  EXPECT_EQ(kStringIntDataSize, map.size());

  Array<String> keys2;
  Array<int> values2;
  map.DecomposeMapTo(&keys2, &values2);
  EXPECT_EQ(0u, map.size());

  EXPECT_EQ(kStringIntDataSize, keys2.size());
  EXPECT_EQ(kStringIntDataSize, values2.size());

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    // We are not guaranteed that the copies have the same sorting as the
    // originals.
    String key = kStringIntData[i].string_data;
    int value = kStringIntData[i].int_data;

    bool found = false;
    for (size_t j = 0; j < keys2.size(); ++j) {
      if (keys2[j] == key) {
        EXPECT_EQ(value, values2[j]);
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found);
  }
}

TEST_F(MapTest, Insert_Copyable) {
  ASSERT_EQ(0u, CopyableType::num_instances());
  mojo::Map<mojo::String, CopyableType> map;
  std::vector<CopyableType*> value_ptrs;

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    const char* key = kStringIntData[i].string_data;
    CopyableType value;
    value_ptrs.push_back(value.ptr());
    map.insert(key, value);
    ASSERT_EQ(i + 1, map.size());
    ASSERT_EQ(i + 1, value_ptrs.size());
    EXPECT_EQ(map.size() + 1, CopyableType::num_instances());
    EXPECT_TRUE(map.at(key).copied());
    EXPECT_EQ(value_ptrs[i], map.at(key).ptr());
    map.at(key).ResetCopied();
    EXPECT_TRUE(map);
  }

  // std::map doesn't have a capacity() method like std::vector so this test is
  // a lot more boring.

  map = nullptr;
  EXPECT_EQ(0u, CopyableType::num_instances());
}

TEST_F(MapTest, Insert_MoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  mojo::Map<mojo::String, MoveOnlyType> map;
  std::vector<MoveOnlyType*> value_ptrs;

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    const char* key = kStringIntData[i].string_data;
    MoveOnlyType value;
    value_ptrs.push_back(value.ptr());
    map.insert(key, std::move(value));
    ASSERT_EQ(i + 1, map.size());
    ASSERT_EQ(i + 1, value_ptrs.size());
    EXPECT_EQ(map.size() + 1, MoveOnlyType::num_instances());
    EXPECT_TRUE(map.at(key).moved());
    EXPECT_EQ(value_ptrs[i], map.at(key).ptr());
    map.at(key).ResetMoved();
    EXPECT_TRUE(map);
  }

  // std::map doesn't have a capacity() method like std::vector so this test is
  // a lot more boring.

  map = nullptr;
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
}

TEST_F(MapTest, IndexOperator_MoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  mojo::Map<mojo::String, MoveOnlyType> map;
  std::vector<MoveOnlyType*> value_ptrs;

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    const char* key = kStringIntData[i].string_data;
    MoveOnlyType value;
    value_ptrs.push_back(value.ptr());
    map[key] = std::move(value);
    ASSERT_EQ(i + 1, map.size());
    ASSERT_EQ(i + 1, value_ptrs.size());
    EXPECT_EQ(map.size() + 1, MoveOnlyType::num_instances());
    EXPECT_TRUE(map.at(key).moved());
    EXPECT_EQ(value_ptrs[i], map.at(key).ptr());
    map.at(key).ResetMoved();
    EXPECT_TRUE(map);
  }

  // std::map doesn't have a capacity() method like std::vector so this test is
  // a lot more boring.

  map = nullptr;
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
}

TEST_F(MapTest, STLToMojo) {
  std::map<std::string, int> stl_data;
  for (size_t i = 0; i < kStringIntDataSize; ++i)
    stl_data[kStringIntData[i].string_data] = kStringIntData[i].int_data;

  Map<String, int32_t> mojo_data = Map<String, int32_t>::From(stl_data);
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    EXPECT_EQ(kStringIntData[i].int_data,
              mojo_data.at(kStringIntData[i].string_data));
  }
}

TEST_F(MapTest, MojoToSTL) {
  Map<String, int32_t> mojo_map;
  for (size_t i = 0; i < kStringIntDataSize; ++i)
    mojo_map.insert(kStringIntData[i].string_data, kStringIntData[i].int_data);

  std::map<std::string, int> stl_map =
      mojo_map.To<std::map<std::string, int>>();
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    auto it = stl_map.find(kStringIntData[i].string_data);
    ASSERT_TRUE(it != stl_map.end());
    EXPECT_EQ(kStringIntData[i].int_data, it->second);
  }
}

TEST_F(MapTest, MoveFromAndToSTLMap_Copyable) {
  std::map<int32_t, CopyableType> map1;
  map1.insert(std::make_pair(123, CopyableType()));
  map1[123].ResetCopied();

  Map<int32_t, CopyableType> mojo_map(std::move(map1));
  ASSERT_EQ(1u, mojo_map.size());
  ASSERT_NE(mojo_map.end(), mojo_map.find(123));
  ASSERT_FALSE(mojo_map[123].copied());

  std::map<int32_t, CopyableType> map2(mojo_map.PassStorage());
  ASSERT_EQ(1u, map2.size());
  ASSERT_NE(map2.end(), map2.find(123));
  ASSERT_FALSE(map2[123].copied());

  ASSERT_EQ(0u, mojo_map.size());
  ASSERT_TRUE(mojo_map.is_null());
}

TEST_F(MapTest, MoveFromAndToSTLMap_MoveOnly) {
  std::map<int32_t, MoveOnlyType> map1;
  map1.insert(std::make_pair(123, MoveOnlyType()));

  Map<int32_t, MoveOnlyType> mojo_map(std::move(map1));
  ASSERT_EQ(1u, mojo_map.size());
  ASSERT_NE(mojo_map.end(), mojo_map.find(123));

  std::map<int32_t, MoveOnlyType> map2(mojo_map.PassStorage());
  ASSERT_EQ(1u, map2.size());
  ASSERT_NE(map2.end(), map2.find(123));

  ASSERT_EQ(0u, mojo_map.size());
  ASSERT_TRUE(mojo_map.is_null());
}

}  // namespace
}  // namespace test
}  // namespace mojo
