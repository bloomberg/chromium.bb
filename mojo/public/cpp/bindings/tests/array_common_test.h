// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>

#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/tests/container_test_util.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

// Common tests for both mojo::Array and mojo::WTFArray.
template <template <typename...> class ArrayType>
class ArrayCommonTest {
 public:
  // Tests null and empty arrays.
  static void NullAndEmpty() {
    ArrayType<char> array0;
    EXPECT_TRUE(array0.empty());
    EXPECT_FALSE(array0.is_null());
    array0 = nullptr;
    EXPECT_TRUE(array0.is_null());
    EXPECT_FALSE(array0.empty());

    ArrayType<char> array1(nullptr);
    EXPECT_TRUE(array1.is_null());
    EXPECT_FALSE(array1.empty());
    array1.SetToEmpty();
    EXPECT_TRUE(array1.empty());
    EXPECT_FALSE(array1.is_null());
  }

  // Tests that basic array operations work.
  static void Basic() {
    ArrayType<char> array(8);
    for (size_t i = 0; i < array.size(); ++i) {
      char val = static_cast<char>(i * 2);
      array[i] = val;
      EXPECT_EQ(val, array.at(i));
    }
  }

  // Tests that basic ArrayType<bool> operations work.
  static void Bool() {
    ArrayType<bool> array(64);
    for (size_t i = 0; i < array.size(); ++i) {
      bool val = i % 3 == 0;
      array[i] = val;
      EXPECT_EQ(val, array.at(i));
    }
  }

  // Tests that ArrayType<ScopedMessagePipeHandle> supports transferring
  // handles.
  static void Handle() {
    MessagePipe pipe;
    ArrayType<ScopedMessagePipeHandle> handles(2);
    handles[0] = std::move(pipe.handle0);
    handles[1].reset(pipe.handle1.release());

    EXPECT_FALSE(pipe.handle0.is_valid());
    EXPECT_FALSE(pipe.handle1.is_valid());

    ArrayType<ScopedMessagePipeHandle> handles2 = std::move(handles);
    EXPECT_TRUE(handles2[0].is_valid());
    EXPECT_TRUE(handles2[1].is_valid());

    ScopedMessagePipeHandle pipe_handle = std::move(handles2[0]);
    EXPECT_TRUE(pipe_handle.is_valid());
    EXPECT_FALSE(handles2[0].is_valid());
  }

  // Tests that ArrayType<ScopedMessagePipeHandle> supports closing handles.
  static void HandlesAreClosed() {
    MessagePipe pipe;
    MojoHandle pipe0_value = pipe.handle0.get().value();
    MojoHandle pipe1_value = pipe.handle0.get().value();

    {
      ArrayType<ScopedMessagePipeHandle> handles(2);
      handles[0] = std::move(pipe.handle0);
      handles[1].reset(pipe.handle0.release());
    }

    // We expect the pipes to have been closed.
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe0_value));
    EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe1_value));
  }

  static void Clone() {
    {
      // Test POD.
      ArrayType<int32_t> array(3);
      for (size_t i = 0; i < array.size(); ++i)
        array[i] = static_cast<int32_t>(i);

      ArrayType<int32_t> clone_array = array.Clone();
      EXPECT_EQ(array.size(), clone_array.size());
      for (size_t i = 0; i < array.size(); ++i)
        EXPECT_EQ(array[i], clone_array[i]);
    }

    {
      // Test copyable object.
      ArrayType<String> array(2);
      array[0] = "hello";
      array[1] = "world";

      ArrayType<String> clone_array = array.Clone();
      EXPECT_EQ(array.size(), clone_array.size());
      for (size_t i = 0; i < array.size(); ++i)
        EXPECT_EQ(array[i], clone_array[i]);
    }

    {
      // Test struct.
      ArrayType<RectPtr> array(2);
      array[1] = Rect::New();
      array[1]->x = 1;
      array[1]->y = 2;
      array[1]->width = 3;
      array[1]->height = 4;

      ArrayType<RectPtr> clone_array = array.Clone();
      EXPECT_EQ(array.size(), clone_array.size());
      EXPECT_TRUE(clone_array[0].is_null());
      EXPECT_EQ(array[1]->x, clone_array[1]->x);
      EXPECT_EQ(array[1]->y, clone_array[1]->y);
      EXPECT_EQ(array[1]->width, clone_array[1]->width);
      EXPECT_EQ(array[1]->height, clone_array[1]->height);
    }

    {
      // Test array of array.
      ArrayType<ArrayType<int8_t>> array(2);
      array[0] = nullptr;
      array[1] = ArrayType<int8_t>(2);
      array[1][0] = 0;
      array[1][1] = 1;

      ArrayType<ArrayType<int8_t>> clone_array = array.Clone();
      EXPECT_EQ(array.size(), clone_array.size());
      EXPECT_TRUE(clone_array[0].is_null());
      EXPECT_EQ(array[1].size(), clone_array[1].size());
      EXPECT_EQ(array[1][0], clone_array[1][0]);
      EXPECT_EQ(array[1][1], clone_array[1][1]);
    }

    {
      // Test that array of handles still works although Clone() is not
      // available.
      ArrayType<ScopedMessagePipeHandle> array(10);
      EXPECT_FALSE(array[0].is_valid());
    }
  }

  static void Serialization_ArrayOfPOD() {
    ArrayType<int32_t> array(4);
    for (size_t i = 0; i < array.size(); ++i)
      array[i] = static_cast<int32_t>(i);

    size_t size =
        mojo::internal::PrepareToSerialize<Array<int32_t>>(array, nullptr);
    EXPECT_EQ(8U + 4 * 4U, size);

    mojo::internal::FixedBufferForTesting buf(size);
    mojo::internal::Array_Data<int32_t>* data;
    mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
    mojo::internal::Serialize<Array<int32_t>>(array, &buf, &data,
                                              &validate_params, nullptr);

    ArrayType<int32_t> array2;
    mojo::internal::Deserialize<Array<int32_t>>(data, &array2, nullptr);

    EXPECT_EQ(4U, array2.size());
    for (size_t i = 0; i < array2.size(); ++i)
      EXPECT_EQ(static_cast<int32_t>(i), array2[i]);
  }

  static void Serialization_EmptyArrayOfPOD() {
    ArrayType<int32_t> array;
    size_t size =
        mojo::internal::PrepareToSerialize<Array<int32_t>>(array, nullptr);
    EXPECT_EQ(8U, size);

    mojo::internal::FixedBufferForTesting buf(size);
    mojo::internal::Array_Data<int32_t>* data;
    mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
    mojo::internal::Serialize<Array<int32_t>>(array, &buf, &data,
                                              &validate_params, nullptr);

    ArrayType<int32_t> array2;
    mojo::internal::Deserialize<Array<int32_t>>(data, &array2, nullptr);
    EXPECT_EQ(0U, array2.size());
  }

  static void Serialization_ArrayOfArrayOfPOD() {
    ArrayType<ArrayType<int32_t>> array(2);
    for (size_t j = 0; j < array.size(); ++j) {
      ArrayType<int32_t> inner(4);
      for (size_t i = 0; i < inner.size(); ++i)
        inner[i] = static_cast<int32_t>(i + (j * 10));
      array[j] = std::move(inner);
    }

    size_t size = mojo::internal::PrepareToSerialize<Array<Array<int32_t>>>(
        array, nullptr);
    EXPECT_EQ(8U + 2 * 8U + 2 * (8U + 4 * 4U), size);

    mojo::internal::FixedBufferForTesting buf(size);
    mojo::internal::Array_Data<mojo::internal::Array_Data<int32_t>*>* data;
    mojo::internal::ArrayValidateParams validate_params(
        0, false, new mojo::internal::ArrayValidateParams(0, false, nullptr));
    mojo::internal::Serialize<Array<Array<int32_t>>>(array, &buf, &data,
                                                     &validate_params, nullptr);

    ArrayType<ArrayType<int32_t>> array2;
    mojo::internal::Deserialize<Array<Array<int32_t>>>(data, &array2, nullptr);

    EXPECT_EQ(2U, array2.size());
    for (size_t j = 0; j < array2.size(); ++j) {
      const ArrayType<int32_t>& inner = array2[j];
      EXPECT_EQ(4U, inner.size());
      for (size_t i = 0; i < inner.size(); ++i)
        EXPECT_EQ(static_cast<int32_t>(i + (j * 10)), inner[i]);
    }
  }

  static void Serialization_ArrayOfBool() {
    ArrayType<bool> array(10);
    for (size_t i = 0; i < array.size(); ++i)
      array[i] = i % 2 ? true : false;

    size_t size =
        mojo::internal::PrepareToSerialize<Array<bool>>(array, nullptr);
    EXPECT_EQ(8U + 8U, size);

    mojo::internal::FixedBufferForTesting buf(size);
    mojo::internal::Array_Data<bool>* data;
    mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
    mojo::internal::Serialize<Array<bool>>(array, &buf, &data, &validate_params,
                                           nullptr);

    ArrayType<bool> array2;
    mojo::internal::Deserialize<Array<bool>>(data, &array2, nullptr);

    EXPECT_EQ(10U, array2.size());
    for (size_t i = 0; i < array2.size(); ++i)
      EXPECT_EQ(i % 2 ? true : false, array2[i]);
  }

  static void Serialization_ArrayOfString() {
    ArrayType<String> array(10);
    for (size_t i = 0; i < array.size(); ++i) {
      char c = 'A' + static_cast<char>(i);
      array[i] = String(&c, 1);
    }

    size_t size =
        mojo::internal::PrepareToSerialize<Array<String>>(array, nullptr);
    EXPECT_EQ(8U +            // array header
                  10 * 8U +   // array payload (10 pointers)
                  10 * (8U +  // string header
                        8U),  // string length of 1 padded to 8
              size);

    mojo::internal::FixedBufferForTesting buf(size);
    mojo::internal::Array_Data<mojo::internal::String_Data*>* data;
    mojo::internal::ArrayValidateParams validate_params(
        0, false, new mojo::internal::ArrayValidateParams(0, false, nullptr));
    mojo::internal::Serialize<Array<String>>(array, &buf, &data,
                                             &validate_params, nullptr);

    ArrayType<String> array2;
    mojo::internal::Deserialize<Array<String>>(data, &array2, nullptr);

    EXPECT_EQ(10U, array2.size());
    for (size_t i = 0; i < array2.size(); ++i) {
      char c = 'A' + static_cast<char>(i);
      EXPECT_EQ(String(&c, 1), array2[i]);
    }
  }

  static void Resize_Copyable() {
    ASSERT_EQ(0u, CopyableType::num_instances());
    ArrayType<CopyableType> array(3);
    std::vector<CopyableType*> value_ptrs;
    value_ptrs.push_back(array[0].ptr());
    value_ptrs.push_back(array[1].ptr());

    for (size_t i = 0; i < array.size(); i++)
      array[i].ResetCopied();

    array.resize(2);
    ASSERT_EQ(2u, array.size());
    EXPECT_EQ(array.size(), CopyableType::num_instances());
    for (size_t i = 0; i < array.size(); i++) {
      EXPECT_FALSE(array[i].copied());
      EXPECT_EQ(value_ptrs[i], array[i].ptr());
    }

    array.resize(3);
    array[2].ResetCopied();
    ASSERT_EQ(3u, array.size());
    EXPECT_EQ(array.size(), CopyableType::num_instances());
    for (size_t i = 0; i < array.size(); i++)
      EXPECT_FALSE(array[i].copied());
    value_ptrs.push_back(array[2].ptr());

    size_t capacity = array.storage().capacity();
    array.resize(capacity);
    ASSERT_EQ(capacity, array.size());
    EXPECT_EQ(array.size(), CopyableType::num_instances());
    for (size_t i = 0; i < 3; i++)
      EXPECT_FALSE(array[i].copied());
    for (size_t i = 3; i < array.size(); i++) {
      array[i].ResetCopied();
      value_ptrs.push_back(array[i].ptr());
    }

    array.resize(capacity + 2);
    ASSERT_EQ(capacity + 2, array.size());
    EXPECT_EQ(array.size(), CopyableType::num_instances());
    for (size_t i = 0; i < capacity; i++) {
      EXPECT_TRUE(array[i].copied());
      EXPECT_EQ(value_ptrs[i], array[i].ptr());
    }
    array = nullptr;
    EXPECT_EQ(0u, CopyableType::num_instances());
    EXPECT_FALSE(array);
    array.resize(0);
    EXPECT_EQ(0u, CopyableType::num_instances());
    EXPECT_TRUE(array);
  }

  static void Resize_MoveOnly() {
    ASSERT_EQ(0u, MoveOnlyType::num_instances());
    ArrayType<MoveOnlyType> array(3);
    std::vector<MoveOnlyType*> value_ptrs;
    value_ptrs.push_back(array[0].ptr());
    value_ptrs.push_back(array[1].ptr());

    for (size_t i = 0; i < array.size(); i++)
      EXPECT_FALSE(array[i].moved());

    array.resize(2);
    ASSERT_EQ(2u, array.size());
    EXPECT_EQ(array.size(), MoveOnlyType::num_instances());
    for (size_t i = 0; i < array.size(); i++) {
      EXPECT_FALSE(array[i].moved());
      EXPECT_EQ(value_ptrs[i], array[i].ptr());
    }

    array.resize(3);
    ASSERT_EQ(3u, array.size());
    EXPECT_EQ(array.size(), MoveOnlyType::num_instances());
    for (size_t i = 0; i < array.size(); i++)
      EXPECT_FALSE(array[i].moved());
    value_ptrs.push_back(array[2].ptr());

    size_t capacity = array.storage().capacity();
    array.resize(capacity);
    ASSERT_EQ(capacity, array.size());
    EXPECT_EQ(array.size(), MoveOnlyType::num_instances());
    for (size_t i = 0; i < array.size(); i++)
      EXPECT_FALSE(array[i].moved());
    for (size_t i = 3; i < array.size(); i++)
      value_ptrs.push_back(array[i].ptr());

    array.resize(capacity + 2);
    ASSERT_EQ(capacity + 2, array.size());
    EXPECT_EQ(array.size(), MoveOnlyType::num_instances());
    for (size_t i = 0; i < capacity; i++) {
      EXPECT_TRUE(array[i].moved());
      EXPECT_EQ(value_ptrs[i], array[i].ptr());
    }
    for (size_t i = capacity; i < array.size(); i++)
      EXPECT_FALSE(array[i].moved());

    array = nullptr;
    EXPECT_EQ(0u, MoveOnlyType::num_instances());
    EXPECT_FALSE(array);
    array.resize(0);
    EXPECT_EQ(0u, MoveOnlyType::num_instances());
    EXPECT_TRUE(array);
  }
};

#define ARRAY_COMMON_TEST(ArrayType, test_name) \
  TEST_F(ArrayType##Test, test_name) {          \
    ArrayCommonTest<ArrayType>::test_name();    \
  }

}  // namespace test
}  // namespace mojo
