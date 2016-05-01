// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/bounds_checker.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "mojo/public/interfaces/bindings/tests/test_unions.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

TEST(UnionTest, PlainOldDataGetterSetter) {
  PodUnionPtr pod(PodUnion::New());

  pod->set_f_int8(10);
  EXPECT_EQ(10, pod->get_f_int8());
  EXPECT_TRUE(pod->is_f_int8());
  EXPECT_FALSE(pod->is_f_int8_other());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT8);

  pod->set_f_uint8(11);
  EXPECT_EQ(11, pod->get_f_uint8());
  EXPECT_TRUE(pod->is_f_uint8());
  EXPECT_FALSE(pod->is_f_int8());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT8);

  pod->set_f_int16(12);
  EXPECT_EQ(12, pod->get_f_int16());
  EXPECT_TRUE(pod->is_f_int16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT16);

  pod->set_f_uint16(13);
  EXPECT_EQ(13, pod->get_f_uint16());
  EXPECT_TRUE(pod->is_f_uint16());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT16);

  pod->set_f_int32(14);
  EXPECT_EQ(14, pod->get_f_int32());
  EXPECT_TRUE(pod->is_f_int32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT32);

  pod->set_f_uint32(static_cast<uint32_t>(15));
  EXPECT_EQ(static_cast<uint32_t>(15), pod->get_f_uint32());
  EXPECT_TRUE(pod->is_f_uint32());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT32);

  pod->set_f_int64(16);
  EXPECT_EQ(16, pod->get_f_int64());
  EXPECT_TRUE(pod->is_f_int64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_INT64);

  pod->set_f_uint64(static_cast<uint64_t>(17));
  EXPECT_EQ(static_cast<uint64_t>(17), pod->get_f_uint64());
  EXPECT_TRUE(pod->is_f_uint64());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_UINT64);

  pod->set_f_float(1.5);
  EXPECT_EQ(1.5, pod->get_f_float());
  EXPECT_TRUE(pod->is_f_float());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_FLOAT);

  pod->set_f_double(1.9);
  EXPECT_EQ(1.9, pod->get_f_double());
  EXPECT_TRUE(pod->is_f_double());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_DOUBLE);

  pod->set_f_bool(true);
  EXPECT_TRUE(pod->get_f_bool());
  pod->set_f_bool(false);
  EXPECT_FALSE(pod->get_f_bool());
  EXPECT_TRUE(pod->is_f_bool());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_BOOL);

  pod->set_f_enum(AnEnum::SECOND);
  EXPECT_EQ(AnEnum::SECOND, pod->get_f_enum());
  EXPECT_TRUE(pod->is_f_enum());
  EXPECT_EQ(pod->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PodEquals) {
  PodUnionPtr pod1(PodUnion::New());
  PodUnionPtr pod2(PodUnion::New());

  pod1->set_f_int8(10);
  pod2->set_f_int8(10);
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_int8(11);
  EXPECT_FALSE(pod1.Equals(pod2));

  pod2->set_f_int8_other(10);
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, PodClone) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  PodUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(10, pod_clone->get_f_int8());
  EXPECT_TRUE(pod_clone->is_f_int8());
  EXPECT_EQ(pod_clone->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, PodSerialization) {
  PodUnionPtr pod1(PodUnion::New());
  pod1->set_f_int8(10);

  size_t size = GetSerializedSize_(pod1, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod1), &buf, &data, false, nullptr);

  PodUnionPtr pod2;
  Deserialize_(data, &pod2, nullptr);

  EXPECT_EQ(10, pod2->get_f_int8());
  EXPECT_TRUE(pod2->is_f_int8());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, EnumSerialization) {
  PodUnionPtr pod1(PodUnion::New());
  pod1->set_f_enum(AnEnum::SECOND);

  size_t size = GetSerializedSize_(pod1, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod1), &buf, &data, false, nullptr);

  PodUnionPtr pod2;
  Deserialize_(data, &pod2, nullptr);

  EXPECT_EQ(AnEnum::SECOND, pod2->get_f_enum());
  EXPECT_TRUE(pod2->is_f_enum());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PodValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  size_t size = GetSerializedSize_(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod), &buf, &data, false, nullptr);
  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, SerializeNotNull) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(0);
  size_t size = GetSerializedSize_(pod, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod), &buf, &data, false, nullptr);
  EXPECT_FALSE(data->is_null());
}

TEST(UnionTest, SerializeIsNullInlined) {
  PodUnionPtr pod;
  size_t size = GetSerializedSize_(pod, false, nullptr);
  EXPECT_EQ(16U, size);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);

  // Check that dirty output buffers are handled correctly by serialization.
  data->size = 16U;
  data->tag = PodUnion::Tag::F_UINT16;
  data->data.f_f_int16 = 20;

  SerializeUnion_(std::move(pod), &buf, &data, true, nullptr);
  EXPECT_TRUE(data->is_null());

  PodUnionPtr pod2;
  Deserialize_(data, &pod2, nullptr);
  EXPECT_TRUE(pod2.is_null());
}

TEST(UnionTest, SerializeIsNullNotInlined) {
  PodUnionPtr pod;
  size_t size = GetSerializedSize_(pod, false, nullptr);
  EXPECT_EQ(16U, size);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod), &buf, &data, false, nullptr);
  EXPECT_EQ(nullptr, data);
}

TEST(UnionTest, NullValidation) {
  void* buf = nullptr;
  mojo::internal::BoundsChecker bounds_checker(buf, 0, 0);
  EXPECT_TRUE(internal::PodUnion_Data::Validate(buf, &bounds_checker, false));
}

TEST(UnionTest, OutOfAlignmentValidation) {
  size_t size = sizeof(internal::PodUnion_Data);
  // Get an aligned object and shift the alignment.
  mojo::internal::FixedBufferForTesting aligned_buf(size + 1);
  void* raw_buf = aligned_buf.Leak();
  char* buf = reinterpret_cast<char*>(raw_buf) + 1;

  internal::PodUnion_Data* data =
      reinterpret_cast<internal::PodUnion_Data*>(buf);
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, OOBValidation) {
  size_t size = sizeof(internal::PodUnion_Data) - 1;
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownTagValidation) {
  size_t size = sizeof(internal::PodUnion_Data);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);
  data->tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(0xFFFFFF);
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownEnumValueValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_enum(static_cast<AnEnum>(0xFFFF));

  size_t size = GetSerializedSize_(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod), &buf, &data, false, nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownExtensibleEnumValueValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_extensible_enum(static_cast<AnExtensibleEnum>(0xFFFF));

  size_t size = GetSerializedSize_(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod), &buf, &data, false, nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, StringGetterSetter) {
  ObjectUnionPtr pod(ObjectUnion::New());

  String hello("hello world");
  pod->set_f_string(hello);
  EXPECT_EQ(hello, pod->get_f_string());
  EXPECT_TRUE(pod->is_f_string());
  EXPECT_EQ(pod->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, StringEquals) {
  ObjectUnionPtr pod1(ObjectUnion::New());
  ObjectUnionPtr pod2(ObjectUnion::New());

  pod1->set_f_string("hello world");
  pod2->set_f_string("hello world");
  EXPECT_TRUE(pod1.Equals(pod2));

  pod2->set_f_string("hello universe");
  EXPECT_FALSE(pod1.Equals(pod2));
}

TEST(UnionTest, StringClone) {
  ObjectUnionPtr pod(ObjectUnion::New());

  String hello("hello world");
  pod->set_f_string(hello);
  ObjectUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(hello, pod_clone->get_f_string());
  EXPECT_TRUE(pod_clone->is_f_string());
  EXPECT_EQ(pod_clone->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, StringSerialization) {
  ObjectUnionPtr pod1(ObjectUnion::New());

  String hello("hello world");
  pod1->set_f_string(hello);

  size_t size = GetSerializedSize_(pod1, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(pod1), &buf, &data, false, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  ObjectUnionPtr pod2;
  Deserialize_(data, &pod2, nullptr);
  EXPECT_EQ(hello, pod2->get_f_string());
  EXPECT_TRUE(pod2->is_f_string());
  EXPECT_EQ(pod2->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, NullStringValidation) {
  size_t size = sizeof(internal::ObjectUnion_Data);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = internal::ObjectUnion_Data::New(&buf);
  data->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;
  data->data.unknown = 0x0;
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, StringPointerOverflowValidation) {
  size_t size = sizeof(internal::ObjectUnion_Data);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = internal::ObjectUnion_Data::New(&buf);
  data->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;
  data->data.unknown = 0xFFFFFFFFFFFFFFFF;
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, StringValidateOOB) {
  size_t size = 32;
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = internal::ObjectUnion_Data::New(&buf);
  data->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;

  data->data.f_f_string.offset = 8;
  char* ptr = reinterpret_cast<char*>(&data->data.f_f_string);
  mojo::internal::ArrayHeader* array_header =
      reinterpret_cast<mojo::internal::ArrayHeader*>(ptr + *ptr);
  array_header->num_bytes = 20;  // This should go out of bounds.
  array_header->num_elements = 20;
  mojo::internal::BoundsChecker bounds_checker(data, 32, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

// TODO(azani): Move back in array_unittest.cc when possible.
// Array tests
TEST(UnionTest, PodUnionInArray) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_array = Array<PodUnionPtr>(2);
  small_struct->pod_union_array[0] = PodUnion::New();
  small_struct->pod_union_array[1] = PodUnion::New();

  small_struct->pod_union_array[0]->set_f_int8(10);
  small_struct->pod_union_array[1]->set_f_int16(12);

  EXPECT_EQ(10, small_struct->pod_union_array[0]->get_f_int8());
  EXPECT_EQ(12, small_struct->pod_union_array[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerialization) {
  Array<PodUnionPtr> array(2);
  array[0] = PodUnion::New();
  array[1] = PodUnion::New();

  array[0]->set_f_int8(10);
  array[1]->set_f_int16(12);
  EXPECT_EQ(2U, array.size());

  size_t size = GetSerializedSize_(array, nullptr);
  EXPECT_EQ(40U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
  SerializeArray_(std::move(array), &buf, &data, &validate_params, nullptr);

  Array<PodUnionPtr> array2;
  Deserialize_(data, &array2, nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_EQ(12, array2[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerializationWithNull) {
  Array<PodUnionPtr> array(2);
  array[0] = PodUnion::New();

  array[0]->set_f_int8(10);
  EXPECT_EQ(2U, array.size());

  size_t size = GetSerializedSize_(array, nullptr);
  EXPECT_EQ(40U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  mojo::internal::ArrayValidateParams validate_params(0, true, nullptr);
  SerializeArray_(std::move(array), &buf, &data, &validate_params, nullptr);

  Array<PodUnionPtr> array2;
  Deserialize_(data, &array2, nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_TRUE(array2[1].is_null());
}

TEST(UnionTest, ObjectUnionInArraySerialization) {
  Array<ObjectUnionPtr> array(2);
  array[0] = ObjectUnion::New();
  array[1] = ObjectUnion::New();

  array[0]->set_f_string("hello");
  array[1]->set_f_string("world");
  EXPECT_EQ(2U, array.size());

  size_t size = GetSerializedSize_(array, nullptr);
  EXPECT_EQ(72U, size);

  mojo::internal::FixedBufferForTesting buf(size);

  mojo::internal::Array_Data<internal::ObjectUnion_Data>* data;
  mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
  SerializeArray_(std::move(array), &buf, &data, &validate_params, nullptr);

  data->EncodePointers();

  std::vector<char> new_buf;
  new_buf.resize(size);

  void* raw_buf = buf.Leak();
  memcpy(new_buf.data(), raw_buf, size);
  free(raw_buf);

  data =
      reinterpret_cast<mojo::internal::Array_Data<internal::ObjectUnion_Data>*>(
          new_buf.data());
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  ASSERT_TRUE(mojo::internal::Array_Data<internal::ObjectUnion_Data>::Validate(
      data, &bounds_checker, &validate_params));

  data->DecodePointers();
  Array<ObjectUnionPtr> array2;
  Deserialize_(data, &array2, nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ(String("hello"), array2[0]->get_f_string());
  EXPECT_EQ(String("world"), array2[1]->get_f_string());
}

// TODO(azani): Move back in struct_unittest.cc when possible.
// Struct tests
TEST(UnionTest, Clone_Union) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int8(10);

  SmallStructPtr clone = small_struct.Clone();
  EXPECT_EQ(10, clone->pod_union->get_f_int8());
}

// Serialization test of a struct with a union of plain old data.
TEST(UnionTest, Serialization_UnionOfPods) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  size_t size = GetSerializedSize_(small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  Serialize_(std::move(small_struct), &buf, &data, nullptr);

  SmallStructPtr deserialized;
  Deserialize_(data, &deserialized, nullptr);

  EXPECT_EQ(10, deserialized->pod_union->get_f_int32());
}

// Serialization test of a struct with a union of structs.
TEST(UnionTest, Serialization_UnionOfObjects) {
  SmallObjStructPtr obj_struct(SmallObjStruct::New());
  obj_struct->obj_union = ObjectUnion::New();
  String hello("hello world");
  obj_struct->obj_union->set_f_string(hello);

  size_t size = GetSerializedSize_(obj_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallObjStruct_Data* data = nullptr;
  Serialize_(std::move(obj_struct), &buf, &data, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  SmallObjStructPtr deserialized;
  Deserialize_(data, &deserialized, nullptr);

  EXPECT_EQ(hello, deserialized->obj_union->get_f_string());
}

// Validation test of a struct with a union.
TEST(UnionTest, Validation_UnionsInStruct) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  size_t size = GetSerializedSize_(small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  Serialize_(std::move(small_struct), &buf, &data, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(raw_buf, &bounds_checker));
  free(raw_buf);
}

// Validation test of a struct union fails due to unknown union tag.
TEST(UnionTest, Validation_PodUnionInStruct_Failure) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  size_t size = GetSerializedSize_(small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  Serialize_(std::move(small_struct), &buf, &data, nullptr);
  data->pod_union.tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(100);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(internal::SmallStruct_Data::Validate(raw_buf, &bounds_checker));
  free(raw_buf);
}

// Validation fails due to non-nullable null union in struct.
TEST(UnionTest, Validation_NullUnion_Failure) {
  SmallStructNonNullableUnionPtr small_struct(
      SmallStructNonNullableUnion::New());

  size_t size = GetSerializedSize_(small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStructNonNullableUnion_Data* data =
      internal::SmallStructNonNullableUnion_Data::New(&buf);

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(internal::SmallStructNonNullableUnion_Data::Validate(
      raw_buf, &bounds_checker));
  free(raw_buf);
}

// Validation passes with nullable null union.
TEST(UnionTest, Validation_NullableUnion) {
  SmallStructPtr small_struct(SmallStruct::New());

  size_t size = GetSerializedSize_(small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  Serialize_(std::move(small_struct), &buf, &data, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(raw_buf, &bounds_checker));
  free(raw_buf);
}

// TODO(azani): Move back in map_unittest.cc when possible.
// Map Tests
TEST(UnionTest, PodUnionInMap) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_map = Map<String, PodUnionPtr>();
  small_struct->pod_union_map.insert("one", PodUnion::New());
  small_struct->pod_union_map.insert("two", PodUnion::New());

  small_struct->pod_union_map["one"]->set_f_int8(8);
  small_struct->pod_union_map["two"]->set_f_int16(16);

  EXPECT_EQ(8, small_struct->pod_union_map["one"]->get_f_int8());
  EXPECT_EQ(16, small_struct->pod_union_map["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerialization) {
  Map<String, PodUnionPtr> map;
  map.insert("one", PodUnion::New());
  map.insert("two", PodUnion::New());

  map["one"]->set_f_int8(8);
  map["two"]->set_f_int16(16);

  size_t size = GetSerializedSize_(map, nullptr);
  EXPECT_EQ(120U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Map_Data<mojo::internal::String_Data*,
                           internal::PodUnion_Data>* data;
  mojo::internal::ArrayValidateParams validate_params(0, false, nullptr);
  SerializeMap_(std::move(map), &buf, &data, &validate_params, nullptr);

  Map<String, PodUnionPtr> map2;
  Deserialize_(data, &map2, nullptr);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_EQ(16, map2["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerializationWithNull) {
  Map<String, PodUnionPtr> map;
  map.insert("one", PodUnion::New());
  map.insert("two", nullptr);

  map["one"]->set_f_int8(8);

  size_t size = GetSerializedSize_(map, nullptr);
  EXPECT_EQ(120U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Map_Data<mojo::internal::String_Data*,
                           internal::PodUnion_Data>* data;
  mojo::internal::ArrayValidateParams validate_params(0, true, nullptr);
  SerializeMap_(std::move(map), &buf, &data, &validate_params, nullptr);

  Map<String, PodUnionPtr> map2;
  Deserialize_(data, &map2, nullptr);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_TRUE(map2["two"].is_null());
}

TEST(UnionTest, StructInUnionGetterSetterPasser) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  EXPECT_EQ(8, obj->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionSerialization) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  ObjectUnionPtr obj2;
  Deserialize_(data, &obj2, nullptr);
  EXPECT_EQ(8, obj2->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionValidation) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  size_t size = GetSerializedSize_(obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, StructInUnionValidationNonNullable) {
  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  size_t size = GetSerializedSize_(obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, StructInUnionValidationNullable) {
  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_nullable(std::move(dummy));

  size_t size = GetSerializedSize_(obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, ArrayInUnionGetterSetter) {
  Array<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  EXPECT_EQ(8, obj->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionSerialization) {
  Array<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  ObjectUnionPtr obj2;
  Deserialize_(data, &obj2, nullptr);

  EXPECT_EQ(8, obj2->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj2->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionValidation) {
  Array<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);

  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, MapInUnionGetterSetter) {
  Map<String, int8_t> map;
  map.insert("one", 1);
  map.insert("two", 2);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  EXPECT_EQ(1, obj->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionSerialization) {
  Map<String, int8_t> map;
  map.insert("one", 1);
  map.insert("two", 2);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(112U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  ObjectUnionPtr obj2;
  Deserialize_(data, &obj2, nullptr);

  EXPECT_EQ(1, obj2->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj2->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionValidation) {
  Map<String, int8_t> map;
  map.insert("one", 1);
  map.insert("two", 2);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(112U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);

  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, UnionInUnionGetterSetter) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  EXPECT_EQ(10, obj->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionSerialization) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();
  data->DecodePointers();

  ObjectUnionPtr obj2;
  Deserialize_(data, &obj2, nullptr);
  EXPECT_EQ(10, obj2->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  size_t size = GetSerializedSize_(obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);

  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_TRUE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, UnionInUnionValidationNonNullable) {
  PodUnionPtr pod(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  size_t size = GetSerializedSize_(obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  SerializeUnion_(std::move(obj), &buf, &data, false, nullptr);
  data->EncodePointers();

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 0);
  EXPECT_FALSE(
      internal::ObjectUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, HandleInUnionGetterSetter) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

  std::string golden("hello world");
  WriteTextMessage(pipe0.get(), golden);

  std::string actual;
  ReadTextMessage(handle->get_f_message_pipe().get(), &actual);

  EXPECT_EQ(golden, actual);
}

TEST(UnionTest, HandleInUnionSerialization) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

  mojo::internal::SerializationContext context;
  size_t size = GetSerializedSize_(handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  SerializeUnion_(std::move(handle), &buf, &data, false, &context);
  EXPECT_EQ(1U, context.handles.size());

  HandleUnionPtr handle2(HandleUnion::New());
  Deserialize_(data, &handle2, &context);

  std::string golden("hello world");
  WriteTextMessage(pipe0.get(), golden);

  std::string actual;
  ReadTextMessage(handle2->get_f_message_pipe().get(), &actual);

  EXPECT_EQ(golden, actual);
}

TEST(UnionTest, HandleInUnionValidation) {
  ScopedMessagePipeHandle pipe0;
  ScopedMessagePipeHandle pipe1;

  CreateMessagePipe(nullptr, &pipe0, &pipe1);

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe1));

  mojo::internal::SerializationContext context;
  size_t size = GetSerializedSize_(handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  SerializeUnion_(std::move(handle), &buf, &data, false, &context);

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 1);
  EXPECT_TRUE(
      internal::HandleUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

TEST(UnionTest, HandleInUnionValidationNull) {
  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe));

  mojo::internal::SerializationContext context;
  size_t size = GetSerializedSize_(handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  SerializeUnion_(std::move(handle), &buf, &data, false, &context);

  void* raw_buf = buf.Leak();
  mojo::internal::BoundsChecker bounds_checker(data,
                                               static_cast<uint32_t>(size), 1);
  EXPECT_FALSE(
      internal::HandleUnion_Data::Validate(raw_buf, &bounds_checker, false));
  free(raw_buf);
}

class SmallCacheImpl : public SmallCache {
 public:
  explicit SmallCacheImpl(const base::Closure& closure)
      : int_value_(0), closure_(closure) {}
  ~SmallCacheImpl() override {}
  int64_t int_value() const { return int_value_; }

 private:
  void SetIntValue(int64_t int_value) override {
    int_value_ = int_value;
    closure_.Run();
  }
  void GetIntValue(const GetIntValueCallback& callback) override {
    callback.Run(int_value_);
  }

  int64_t int_value_;
  base::Closure closure_;
};

TEST(UnionTest, InterfaceInUnion) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  SmallCacheImpl impl(run_loop.QuitClosure());
  SmallCachePtr ptr;
  Binding<SmallCache> bindings(&impl, GetProxy(&ptr));

  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_small_cache(std::move(ptr));

  handle->get_f_small_cache()->SetIntValue(10);
  run_loop.Run();
  EXPECT_EQ(10, impl.int_value());
}

TEST(UnionTest, InterfaceInUnionSerialization) {
  base::MessageLoop message_loop;
  base::RunLoop run_loop;
  SmallCacheImpl impl(run_loop.QuitClosure());
  SmallCachePtr ptr;
  Binding<SmallCache> bindings(&impl, GetProxy(&ptr));

  mojo::internal::SerializationContext context;
  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_small_cache(std::move(ptr));
  size_t size = GetSerializedSize_(handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  SerializeUnion_(std::move(handle), &buf, &data, false, &context);
  EXPECT_EQ(1U, context.handles.size());

  HandleUnionPtr handle2(HandleUnion::New());
  Deserialize_(data, &handle2, &context);

  handle2->get_f_small_cache()->SetIntValue(10);
  run_loop.Run();
  EXPECT_EQ(10, impl.int_value());
}

class UnionInterfaceImpl : public UnionInterface {
 public:
  UnionInterfaceImpl() {}
  ~UnionInterfaceImpl() override {}

 private:
  void Echo(PodUnionPtr in, const EchoCallback& callback) override {
    callback.Run(std::move(in));
  }
};

TEST(UnionTest, UnionInInterface) {
  base::MessageLoop run_loop;
  UnionInterfaceImpl impl;
  UnionInterfacePtr ptr;
  Binding<UnionInterface> bindings(&impl, GetProxy(&ptr));

  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int16(16);

  ptr->Echo(std::move(pod),
            [](PodUnionPtr out) { EXPECT_EQ(16, out->get_f_int16()); });
  run_loop.RunUntilIdle();
}

}  // namespace test
}  // namespace mojo
