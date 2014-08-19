// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Serialization warnings are only recorded in debug build.
#ifndef NDEBUG

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/interfaces/bindings/tests/serialization_test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using mojo::internal::ArrayValidateParams;
using mojo::internal::NoValidateParams;

// Creates an array of arrays of handles (2 X 3) for testing.
Array<Array<ScopedHandle> > CreateTestNestedHandleArray() {
  Array<Array<ScopedHandle> > array(2);
  for (size_t i = 0; i < array.size(); ++i) {
    Array<ScopedHandle> nested_array(3);
    for (size_t j = 0; j < nested_array.size(); ++j) {
      MessagePipe pipe;
      nested_array[j] = ScopedHandle::From(pipe.handle1.Pass());
    }
    array[i] = nested_array.Pass();
  }

  return array.Pass();
}

class SerializationWarningTest : public testing::Test {
 public:
  virtual ~SerializationWarningTest() {}

 protected:
  template <typename T>
  void TestWarning(T obj, mojo::internal::ValidationError expected_warning) {
    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::internal::FixedBuffer buf(GetSerializedSize_(obj));
    typename T::Data_* data;
    Serialize_(obj.Pass(), &buf, &data);

    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  template <typename ValidateParams, typename T>
  void TestArrayWarning(T obj,
                        mojo::internal::ValidationError expected_warning) {
    warning_observer_.set_last_warning(mojo::internal::VALIDATION_ERROR_NONE);

    mojo::internal::FixedBuffer buf(GetSerializedSize_(obj));
    typename T::Data_* data;
    SerializeArray_<ValidateParams>(obj.Pass(), &buf, &data);

    EXPECT_EQ(expected_warning, warning_observer_.last_warning());
  }

  mojo::internal::SerializationWarningObserverForTesting warning_observer_;
  Environment env_;
};

TEST_F(SerializationWarningTest, HandleInStruct) {
  Struct2Ptr test_struct(Struct2::New());
  EXPECT_FALSE(test_struct->hdl.is_valid());

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);

  test_struct = Struct2::New();
  MessagePipe pipe;
  test_struct->hdl = ScopedHandle::From(pipe.handle1.Pass());

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, StructInStruct) {
  Struct3Ptr test_struct(Struct3::New());
  EXPECT_TRUE(!test_struct->struct_1);

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct3::New();
  test_struct->struct_1 = Struct1::New();

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfStructsInStruct) {
  Struct4Ptr test_struct(Struct4::New());
  EXPECT_TRUE(!test_struct->array);

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->array.resize(1);

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct4::New();
  test_struct->array.resize(0);

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);

  test_struct = Struct4::New();
  test_struct->array.resize(1);
  test_struct->array[0] = Struct1::New();

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, FixedArrayOfStructsInStruct) {
  Struct5Ptr test_struct(Struct5::New());
  EXPECT_TRUE(!test_struct->pair);

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct5::New();
  test_struct->pair.resize(1);
  test_struct->pair[0] = Struct1::New();

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);

  test_struct = Struct5::New();
  test_struct->pair.resize(2);
  test_struct->pair[0] = Struct1::New();
  test_struct->pair[1] = Struct1::New();

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, StringInStruct) {
  Struct6Ptr test_struct(Struct6::New());
  EXPECT_TRUE(!test_struct->str);

  TestWarning(test_struct.Pass(),
              mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_struct = Struct6::New();
  test_struct->str = "hello world";

  TestWarning(test_struct.Pass(), mojo::internal::VALIDATION_ERROR_NONE);
}

TEST_F(SerializationWarningTest, ArrayOfArraysOfHandles) {
  Array<Array<ScopedHandle> > test_array = CreateTestNestedHandleArray();
  test_array[0] = Array<ScopedHandle>();
  test_array[1][0] = ScopedHandle();

  TestArrayWarning<ArrayValidateParams<0, true,
                   ArrayValidateParams<0, true, NoValidateParams> > >(
      test_array.Pass(), mojo::internal::VALIDATION_ERROR_NONE);

  test_array = CreateTestNestedHandleArray();
  test_array[0] = Array<ScopedHandle>();
  TestArrayWarning<ArrayValidateParams<0, false,
                   ArrayValidateParams<0, true, NoValidateParams> > >(
      test_array.Pass(),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_array = CreateTestNestedHandleArray();
  test_array[1][0] = ScopedHandle();
  TestArrayWarning<ArrayValidateParams<0, true,
                   ArrayValidateParams<0, false, NoValidateParams> > >(
      test_array.Pass(),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_INVALID_HANDLE);
}

TEST_F(SerializationWarningTest, ArrayOfStrings) {
  Array<String> test_array(3);
  for (size_t i = 0; i < test_array.size(); ++i)
    test_array[i] = "hello";

  TestArrayWarning<ArrayValidateParams<0, true,
                   ArrayValidateParams<0, false, NoValidateParams> > >(
      test_array.Pass(), mojo::internal::VALIDATION_ERROR_NONE);

  test_array = Array<String>(3);
  TestArrayWarning<ArrayValidateParams<0, false,
                   ArrayValidateParams<0, false, NoValidateParams> > >(
      test_array.Pass(),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_NULL_POINTER);

  test_array = Array<String>(2);
  TestArrayWarning<ArrayValidateParams<3, true,
                   ArrayValidateParams<0, false, NoValidateParams> > >(
      test_array.Pass(),
      mojo::internal::VALIDATION_ERROR_UNEXPECTED_ARRAY_HEADER);
}

}  // namespace
}  // namespace test
}  // namespace mojo

#endif
