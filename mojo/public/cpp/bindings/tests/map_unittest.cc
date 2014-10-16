// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/bindings_internal.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "mojo/public/cpp/environment/environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
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

class MapTest : public testing::Test {
 public:
  virtual ~MapTest() {}

 private:
  Environment env_;
};

// Tests that basic Map operations work.
TEST_F(MapTest, InsertWorks) {
  Map<String, int> map;
  for (size_t i = 0; i < kStringIntDataSize; ++i)
    map.insert(kStringIntData[i].string_data, kStringIntData[i].int_data);

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    EXPECT_EQ(kStringIntData[i].int_data,
              map.at(kStringIntData[i].string_data));
  }
}

TEST_F(MapTest, TestIndexOperator) {
  Map<String, int> map;
  for (size_t i = 0; i < kStringIntDataSize; ++i)
    map[kStringIntData[i].string_data] = kStringIntData[i].int_data;

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    EXPECT_EQ(kStringIntData[i].int_data,
              map.at(kStringIntData[i].string_data));
  }
}

TEST_F(MapTest, TestIndexOperatorAsRValue) {
  Map<String, int> map;
  for (size_t i = 0; i < kStringIntDataSize; ++i)
    map.insert(kStringIntData[i].string_data, kStringIntData[i].int_data);

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    EXPECT_EQ(kStringIntData[i].int_data, map[kStringIntData[i].string_data]);
  }
}

TEST_F(MapTest, TestIndexOperatorMoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  mojo::Map<mojo::String, mojo::Array<int32_t>> map;
  std::vector<MoveOnlyType*> value_ptrs;

  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    const char* key = kStringIntData[i].string_data;
    Array<int32_t> array(1);
    array[0] = kStringIntData[i].int_data;
    map[key] = array.Pass();
    EXPECT_TRUE(map);
  }

  // We now read back that data, to test the behavior of operator[].
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    auto it = map.find(kStringIntData[i].string_data);
    ASSERT_TRUE(it != map.end());
    ASSERT_EQ(1u, it.GetValue().size());
    EXPECT_EQ(kStringIntData[i].int_data, it.GetValue()[0]);
  }
}

TEST_F(MapTest, ConstructedFromArray) {
  Array<String> keys(kStringIntDataSize);
  Array<int> values(kStringIntDataSize);
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    keys[i] = kStringIntData[i].string_data;
    values[i] = kStringIntData[i].int_data;
  }

  Map<String, int> map(keys.Pass(), values.Pass());

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

  Map<String, int> map(keys.Pass(), values.Pass());
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

  map.reset();
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
    map.insert(key, value.Pass());
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

  map.reset();
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
    map[key] = value.Pass();
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

  map.reset();
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

TEST_F(MapTest, MapArrayClone) {
  Map<String, Array<String>> m;
  for (size_t i = 0; i < kStringIntDataSize; ++i) {
    Array<String> s;
    s.push_back(kStringIntData[i].string_data);
    m.insert(kStringIntData[i].string_data, s.Pass());
  }

  Map<String, Array<String>> m2 = m.Clone();

  for (auto it = m2.begin(); it != m2.end(); ++it) {
    ASSERT_EQ(1u, it.GetValue().size());
    EXPECT_EQ(it.GetKey(), it.GetValue().at(0));
  }
}

// Data class for an end-to-end test of serialization. Because making a more
// limited test case tickles a clang compiler bug, we copy a minimal version of
// what our current cpp bindings do.
namespace internal {

class ArrayOfMap_Data {
 public:
  static ArrayOfMap_Data* New(mojo::internal::Buffer* buf) {
    return new (buf->Allocate(sizeof(ArrayOfMap_Data))) ArrayOfMap_Data();
  }

  mojo::internal::StructHeader header_;

  mojo::internal::ArrayPointer<mojo::internal::Map_Data<int32_t, int8_t>*>
      first;
  mojo::internal::ArrayPointer<
      mojo::internal::Map_Data<mojo::internal::String_Data*,
                               mojo::internal::Array_Data<bool>*>*> second;

 private:
  ArrayOfMap_Data() {
    header_.num_bytes = sizeof(*this);
    header_.num_fields = 2;
  }
  ~ArrayOfMap_Data();  // NOT IMPLEMENTED
};
static_assert(sizeof(ArrayOfMap_Data) == 24, "Bad sizeof(ArrayOfMap_Data)");

}  // namespace internal

class ArrayOfMapImpl;
typedef mojo::StructPtr<ArrayOfMapImpl> ArrayOfMapImplPtr;

class ArrayOfMapImpl {
 public:
  typedef internal::ArrayOfMap_Data Data_;
  static ArrayOfMapImplPtr New() {
    ArrayOfMapImplPtr rv;
    mojo::internal::StructHelper<ArrayOfMapImpl>::Initialize(&rv);
    return rv.Pass();
  }

  mojo::Array<mojo::Map<int32_t, int8_t>> first;
  mojo::Array<mojo::Map<mojo::String, mojo::Array<bool>>> second;
};

size_t GetSerializedSize_(const ArrayOfMapImplPtr& input) {
  if (!input)
    return 0;
  size_t size = sizeof(internal::ArrayOfMap_Data);
  size += GetSerializedSize_(input->first);
  size += GetSerializedSize_(input->second);
  return size;
}

void Serialize_(ArrayOfMapImplPtr input,
                mojo::internal::Buffer* buf,
                internal::ArrayOfMap_Data** output) {
  if (input) {
    internal::ArrayOfMap_Data* result = internal::ArrayOfMap_Data::New(buf);
    mojo::SerializeArray_<mojo::internal::ArrayValidateParams<
        0,
        false,
        mojo::internal::
            ArrayValidateParams<0, false, mojo::internal::NoValidateParams>>>(
        mojo::internal::Forward(input->first), buf, &result->first.ptr);
    MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
        !result->first.ptr,
        mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
        "null first field in ArrayOfMapImpl struct");
    mojo::SerializeArray_<mojo::internal::ArrayValidateParams<
        0,
        false,
        mojo::internal::ArrayValidateParams<
            0,
            false,
            mojo::internal::ArrayValidateParams<
                0,
                false,
                mojo::internal::NoValidateParams>>>>(
        mojo::internal::Forward(input->second), buf, &result->second.ptr);
    MOJO_INTERNAL_DLOG_SERIALIZATION_WARNING(
        !result->second.ptr,
        mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER,
        "null second field in ArrayOfMapImpl struct");
    *output = result;
  } else {
    *output = nullptr;
  }
}

void Deserialize_(internal::ArrayOfMap_Data* input, ArrayOfMapImplPtr* output) {
  if (input) {
    ArrayOfMapImplPtr result(ArrayOfMapImpl::New());
    Deserialize_(input->first.ptr, &result->first);
    Deserialize_(input->second.ptr, &result->second);
    *output = result.Pass();
  } else {
    output->reset();
  }
}

TEST_F(MapTest, ArrayOfMap) {
  Array<Map<int32_t, int8_t>> first_array(1);
  first_array[0].insert(1, 42);

  Array<Map<String, Array<bool>>> second_array(1);
  Array<bool> map_value(2);
  map_value[0] = false;
  map_value[1] = true;
  second_array[0].insert("hello world", map_value.Pass());

  ArrayOfMapImplPtr to_pass(ArrayOfMapImpl::New());
  to_pass->first = first_array.Pass();
  to_pass->second = second_array.Pass();

  size_t size = GetSerializedSize_(to_pass);
  mojo::internal::FixedBuffer buf(size);
  internal::ArrayOfMap_Data* data;
  Serialize_(mojo::internal::Forward(to_pass), &buf, &data);

  ArrayOfMapImplPtr to_receive(ArrayOfMapImpl::New());
  Deserialize_(data, &to_receive);

  ASSERT_EQ(1u, to_receive->first.size());
  ASSERT_EQ(1u, to_receive->first[0].size());
  ASSERT_EQ(42, to_receive->first[0].at(1));

  ASSERT_EQ(1u, to_receive->second.size());
  ASSERT_EQ(1u, to_receive->second[0].size());
  ASSERT_FALSE(to_receive->second[0].at("hello world")[0]);
  ASSERT_TRUE(to_receive->second[0].at("hello world")[1]);
}

}  // namespace
}  // namespace test
}  // namespace mojo
