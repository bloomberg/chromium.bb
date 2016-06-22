// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validate_params.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace WTF {
class String;
}

namespace mojo {

template <typename T>
class WTFArray;

template <typename K, typename V>
class WTFMap;

namespace test {
namespace {

struct StringIntData {
  const char* string_data;
  int int_data;
} kStringIntData[] = {
    {"one", 1},
    {"two", 2},
    {"three", 3},
    {"four", 4},
};

const size_t kStringIntDataSize = 4;

}  // namespace

template <template <typename...> class MapType>
struct TypeTraits;

template <>
struct TypeTraits<Map> {
  using StringType = mojo::String;
  template <typename T>
  using ArrayType = Array<T>;
};

template <>
struct TypeTraits<WTFMap> {
  using StringType = WTF::String;
  template <typename T>
  using ArrayType = WTFArray<T>;
};

// Common tests for both mojo::Map and mojo::WTFMap.
template <template <typename...> class MapType>
class MapCommonTest {
 public:
  using StringType = typename TypeTraits<MapType>::StringType;
  template <typename T>
  using ArrayType = typename TypeTraits<MapType>::template ArrayType<T>;

  // Tests null and empty maps.
  static void NullAndEmpty() {
    MapType<char, char> map0;
    EXPECT_TRUE(map0.empty());
    EXPECT_FALSE(map0.is_null());
    map0 = nullptr;
    EXPECT_TRUE(map0.is_null());
    EXPECT_FALSE(map0.empty());

    MapType<char, char> map1(nullptr);
    EXPECT_TRUE(map1.is_null());
    EXPECT_FALSE(map1.empty());
    map1.SetToEmpty();
    EXPECT_TRUE(map1.empty());
    EXPECT_FALSE(map1.is_null());
  }

  // Tests that basic Map operations work.
  static void InsertWorks() {
    MapType<StringType, int> map;
    for (size_t i = 0; i < kStringIntDataSize; ++i)
      map.insert(kStringIntData[i].string_data, kStringIntData[i].int_data);

    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      EXPECT_EQ(kStringIntData[i].int_data,
                map.at(kStringIntData[i].string_data));
    }
  }

  static void TestIndexOperator() {
    MapType<StringType, int> map;
    for (size_t i = 0; i < kStringIntDataSize; ++i)
      map[kStringIntData[i].string_data] = kStringIntData[i].int_data;

    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      EXPECT_EQ(kStringIntData[i].int_data,
                map.at(kStringIntData[i].string_data));
    }
  }

  static void TestIndexOperatorAsRValue() {
    MapType<StringType, int> map;
    for (size_t i = 0; i < kStringIntDataSize; ++i)
      map.insert(kStringIntData[i].string_data, kStringIntData[i].int_data);

    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      EXPECT_EQ(kStringIntData[i].int_data, map[kStringIntData[i].string_data]);
    }
  }

  static void TestIndexOperatorMoveOnly() {
    ASSERT_EQ(0u, MoveOnlyType::num_instances());
    MapType<StringType, ArrayType<int32_t>> map;

    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      const char* key = kStringIntData[i].string_data;
      ArrayType<int32_t> array(1);
      array[0] = kStringIntData[i].int_data;
      map[key] = std::move(array);
      EXPECT_TRUE(map);
    }

    // We now read back that data, to test the behavior of operator[].
    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      auto& value = map[kStringIntData[i].string_data];
      ASSERT_EQ(1u, value.size());
      EXPECT_EQ(kStringIntData[i].int_data, value[0]);
    }
  }

  static void MapArrayClone() {
    MapType<StringType, ArrayType<StringType>> m;
    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      ArrayType<StringType> s(1);
      s[0] = StringType(kStringIntData[i].string_data);
      m.insert(kStringIntData[i].string_data, std::move(s));
    }

    MapType<StringType, ArrayType<StringType>> m2 = m.Clone();

    for (size_t i = 0; i < kStringIntDataSize; ++i) {
      ASSERT_NE(m2.end(), m2.find(kStringIntData[i].string_data));
      ASSERT_EQ(1u, m2[kStringIntData[i].string_data].size());
      EXPECT_EQ(StringType(kStringIntData[i].string_data),
                m2[kStringIntData[i].string_data][0]);
    }
  }

  static void ArrayOfMap() {
    {
      using MojomType = Array<Map<int32_t, int8_t>>;
      using UserType = ArrayType<MapType<int32_t, int8_t>>;

      UserType array(1);
      array[0].insert(1, 42);

      mojo::internal::SerializationContext context;
      size_t size =
          mojo::internal::PrepareToSerialize<MojomType>(array, &context);
      mojo::internal::FixedBufferForTesting buf(size);
      typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
      mojo::internal::ContainerValidateParams validate_params(
          0, false,
          new mojo::internal::ContainerValidateParams(
              new mojo::internal::ContainerValidateParams(0, false, nullptr),
              new mojo::internal::ContainerValidateParams(0, false, nullptr)));

      mojo::internal::Serialize<MojomType>(array, &buf, &data, &validate_params,
                                           &context);

      UserType deserialized_array;
      mojo::internal::Deserialize<MojomType>(data, &deserialized_array,
                                             &context);

      ASSERT_EQ(1u, deserialized_array.size());
      ASSERT_EQ(1u, deserialized_array[0].size());
      ASSERT_EQ(42, deserialized_array[0].at(1));
    }

    {
      using MojomType = Array<Map<String, Array<bool>>>;
      using UserType = ArrayType<MapType<StringType, ArrayType<bool>>>;

      UserType array(1);
      ArrayType<bool> map_value(2);
      map_value[0] = false;
      map_value[1] = true;
      array[0].insert("hello world", std::move(map_value));

      mojo::internal::SerializationContext context;
      size_t size =
          mojo::internal::PrepareToSerialize<MojomType>(array, &context);
      mojo::internal::FixedBufferForTesting buf(size);
      typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
      mojo::internal::ContainerValidateParams validate_params(
          0, false,
          new mojo::internal::ContainerValidateParams(
              new mojo::internal::ContainerValidateParams(
                  0, false, new mojo::internal::ContainerValidateParams(
                                0, false, nullptr)),
              new mojo::internal::ContainerValidateParams(
                  0, false, new mojo::internal::ContainerValidateParams(
                                0, false, nullptr))));
      mojo::internal::Serialize<MojomType>(array, &buf, &data, &validate_params,
                                           &context);

      UserType deserialized_array;
      mojo::internal::Deserialize<MojomType>(data, &deserialized_array,
                                             &context);

      ASSERT_EQ(1u, deserialized_array.size());
      ASSERT_EQ(1u, deserialized_array[0].size());
      ASSERT_FALSE(deserialized_array[0].at("hello world")[0]);
      ASSERT_TRUE(deserialized_array[0].at("hello world")[1]);
    }
  }
};

#define MAP_COMMON_TEST(MapType, test_name) \
  TEST_F(MapType##Test, test_name) { MapCommonTest<MapType>::test_name(); }

}  // namespace test
}  // namespace mojo
