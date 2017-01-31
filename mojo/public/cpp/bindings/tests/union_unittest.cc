// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/bindings/lib/serialization.h"
#include "mojo/public/cpp/bindings/lib/validation_context.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
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

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<PodUnionDataView>(
      pod1, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod1, &buf, &data, false,
                                              &context);

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, &context);

  EXPECT_EQ(10, pod2->get_f_int8());
  EXPECT_TRUE(pod2->is_f_int8());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_INT8);
}

TEST(UnionTest, EnumSerialization) {
  PodUnionPtr pod1(PodUnion::New());
  pod1->set_f_enum(AnEnum::SECOND);

  size_t size = mojo::internal::PrepareToSerialize<PodUnionDataView>(
      pod1, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod1, &buf, &data, false,
                                              nullptr);

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, nullptr);

  EXPECT_EQ(AnEnum::SECOND, pod2->get_f_enum());
  EXPECT_TRUE(pod2->is_f_enum());
  EXPECT_EQ(pod2->which(), PodUnion::Tag::F_ENUM);
}

TEST(UnionTest, PodValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, false, nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, SerializeNotNull) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(0);
  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, false, nullptr);
  EXPECT_FALSE(data->is_null());
}

TEST(UnionTest, SerializeIsNullInlined) {
  PodUnionPtr pod;
  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  EXPECT_EQ(16U, size);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);

  // Check that dirty output buffers are handled correctly by serialization.
  data->size = 16U;
  data->tag = PodUnion::Tag::F_UINT16;
  data->data.f_f_int16 = 20;

  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, true, nullptr);
  EXPECT_TRUE(data->is_null());

  PodUnionPtr pod2;
  mojo::internal::Deserialize<PodUnionDataView>(data, &pod2, nullptr);
  EXPECT_TRUE(pod2.is_null());
}

TEST(UnionTest, SerializeIsNullNotInlined) {
  PodUnionPtr pod;
  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  EXPECT_EQ(16U, size);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, false, nullptr);
  EXPECT_EQ(nullptr, data);
}

TEST(UnionTest, NullValidation) {
  void* buf = nullptr;
  mojo::internal::ValidationContext validation_context(buf, 0, 0, 0);
  EXPECT_TRUE(internal::PodUnion_Data::Validate(
      buf, &validation_context, false));
}

TEST(UnionTest, OutOfAlignmentValidation) {
  size_t size = sizeof(internal::PodUnion_Data);
  // Get an aligned object and shift the alignment.
  mojo::internal::FixedBufferForTesting aligned_buf(size + 1);
  void* raw_buf = aligned_buf.Leak();
  char* buf = reinterpret_cast<char*>(raw_buf) + 1;

  internal::PodUnion_Data* data =
      reinterpret_cast<internal::PodUnion_Data*>(buf);
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::PodUnion_Data::Validate(
      buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, OOBValidation) {
  size_t size = sizeof(internal::PodUnion_Data) - 1;
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownTagValidation) {
  size_t size = sizeof(internal::PodUnion_Data);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = internal::PodUnion_Data::New(&buf);
  data->tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(0xFFFFFF);
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownEnumValueValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_enum(static_cast<AnEnum>(0xFFFF));

  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, false, nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(
      internal::PodUnion_Data::Validate(raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, UnknownExtensibleEnumValueValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_extensible_enum(static_cast<AnExtensibleEnum>(0xFFFF));

  size_t size =
      mojo::internal::PrepareToSerialize<PodUnionDataView>(pod, false, nullptr);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::PodUnion_Data* data = nullptr;
  mojo::internal::Serialize<PodUnionDataView>(pod, &buf, &data, false, nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(
      internal::PodUnion_Data::Validate(raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, StringGetterSetter) {
  ObjectUnionPtr pod(ObjectUnion::New());

  std::string hello("hello world");
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

  std::string hello("hello world");
  pod->set_f_string(hello);
  ObjectUnionPtr pod_clone = pod.Clone();
  EXPECT_EQ(hello, pod_clone->get_f_string());
  EXPECT_TRUE(pod_clone->is_f_string());
  EXPECT_EQ(pod_clone->which(), ObjectUnion::Tag::F_STRING);
}

TEST(UnionTest, StringSerialization) {
  ObjectUnionPtr pod1(ObjectUnion::New());

  std::string hello("hello world");
  pod1->set_f_string(hello);

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      pod1, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(pod1, &buf, &data, false,
                                                 nullptr);

  ObjectUnionPtr pod2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &pod2, nullptr);
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
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, StringPointerOverflowValidation) {
  size_t size = sizeof(internal::ObjectUnion_Data);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = internal::ObjectUnion_Data::New(&buf);
  data->tag = internal::ObjectUnion_Data::ObjectUnion_Tag::F_STRING;
  data->data.unknown = 0xFFFFFFFFFFFFFFFF;
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
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
  mojo::internal::ValidationContext validation_context(data, 32, 0, 0);
  void* raw_buf = buf.Leak();
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

// TODO(azani): Move back in array_unittest.cc when possible.
// Array tests
TEST(UnionTest, PodUnionInArray) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_array.emplace(2);
  small_struct->pod_union_array.value()[0] = PodUnion::New();
  small_struct->pod_union_array.value()[1] = PodUnion::New();

  small_struct->pod_union_array.value()[0]->set_f_int8(10);
  small_struct->pod_union_array.value()[1]->set_f_int16(12);

  EXPECT_EQ(10, small_struct->pod_union_array.value()[0]->get_f_int8());
  EXPECT_EQ(12, small_struct->pod_union_array.value()[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerialization) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::New();
  array[1] = PodUnion::New();

  array[0]->set_f_int8(10);
  array[1]->set_f_int16(12);
  EXPECT_EQ(2U, array.size());

  size_t size =
      mojo::internal::PrepareToSerialize<ArrayDataView<PodUnionDataView>>(
          array, nullptr);
  EXPECT_EQ(40U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  mojo::internal::ContainerValidateParams validate_params(0, false, nullptr);
  mojo::internal::Serialize<ArrayDataView<PodUnionDataView>>(
      array, &buf, &data, &validate_params, nullptr);

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_EQ(12, array2[1]->get_f_int16());
}

TEST(UnionTest, PodUnionInArraySerializationWithNull) {
  std::vector<PodUnionPtr> array(2);
  array[0] = PodUnion::New();

  array[0]->set_f_int8(10);
  EXPECT_EQ(2U, array.size());

  size_t size =
      mojo::internal::PrepareToSerialize<ArrayDataView<PodUnionDataView>>(
          array, nullptr);
  EXPECT_EQ(40U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  mojo::internal::Array_Data<internal::PodUnion_Data>* data;
  mojo::internal::ContainerValidateParams validate_params(0, true, nullptr);
  mojo::internal::Serialize<ArrayDataView<PodUnionDataView>>(
      array, &buf, &data, &validate_params, nullptr);

  std::vector<PodUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<PodUnionDataView>>(data, &array2,
                                                               nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ(10, array2[0]->get_f_int8());
  EXPECT_TRUE(array2[1].is_null());
}

TEST(UnionTest, ObjectUnionInArraySerialization) {
  std::vector<ObjectUnionPtr> array(2);
  array[0] = ObjectUnion::New();
  array[1] = ObjectUnion::New();

  array[0]->set_f_string("hello");
  array[1]->set_f_string("world");
  EXPECT_EQ(2U, array.size());

  size_t size =
      mojo::internal::PrepareToSerialize<ArrayDataView<ObjectUnionDataView>>(
          array, nullptr);
  EXPECT_EQ(72U, size);

  mojo::internal::FixedBufferForTesting buf(size);

  mojo::internal::Array_Data<internal::ObjectUnion_Data>* data;
  mojo::internal::ContainerValidateParams validate_params(0, false, nullptr);
  mojo::internal::Serialize<ArrayDataView<ObjectUnionDataView>>(
      array, &buf, &data, &validate_params, nullptr);

  std::vector<char> new_buf;
  new_buf.resize(size);

  void* raw_buf = buf.Leak();
  memcpy(new_buf.data(), raw_buf, size);
  free(raw_buf);

  data =
      reinterpret_cast<mojo::internal::Array_Data<internal::ObjectUnion_Data>*>(
          new_buf.data());
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  ASSERT_TRUE(mojo::internal::Array_Data<internal::ObjectUnion_Data>::Validate(
      data, &validation_context, &validate_params));

  std::vector<ObjectUnionPtr> array2;
  mojo::internal::Deserialize<ArrayDataView<ObjectUnionDataView>>(data, &array2,
                                                                  nullptr);

  EXPECT_EQ(2U, array2.size());

  EXPECT_EQ("hello", array2[0]->get_f_string());
  EXPECT_EQ("world", array2[1]->get_f_string());
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

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<SmallStructDataView>(
      small_struct, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  mojo::internal::Serialize<SmallStructDataView>(small_struct, &buf, &data,
                                                 &context);

  SmallStructPtr deserialized;
  mojo::internal::Deserialize<SmallStructDataView>(data, &deserialized,
                                                   &context);

  EXPECT_EQ(10, deserialized->pod_union->get_f_int32());
}

// Serialization test of a struct with a union of structs.
TEST(UnionTest, Serialization_UnionOfObjects) {
  SmallObjStructPtr obj_struct(SmallObjStruct::New());
  obj_struct->obj_union = ObjectUnion::New();
  std::string hello("hello world");
  obj_struct->obj_union->set_f_string(hello);

  size_t size = mojo::internal::PrepareToSerialize<SmallObjStructDataView>(
      obj_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallObjStruct_Data* data = nullptr;
  mojo::internal::Serialize<SmallObjStructDataView>(obj_struct, &buf, &data,
                                                    nullptr);

  SmallObjStructPtr deserialized;
  mojo::internal::Deserialize<SmallObjStructDataView>(data, &deserialized,
                                                      nullptr);

  EXPECT_EQ(hello, deserialized->obj_union->get_f_string());
}

// Validation test of a struct with a union.
TEST(UnionTest, Validation_UnionsInStruct) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<SmallStructDataView>(
      small_struct, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  mojo::internal::Serialize<SmallStructDataView>(small_struct, &buf, &data,
                                                 &context);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(
      raw_buf, &validation_context));
  free(raw_buf);
}

// Validation test of a struct union fails due to unknown union tag.
TEST(UnionTest, Validation_PodUnionInStruct_Failure) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union = PodUnion::New();
  small_struct->pod_union->set_f_int32(10);

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<SmallStructDataView>(
      small_struct, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  mojo::internal::Serialize<SmallStructDataView>(small_struct, &buf, &data,
                                                 &context);
  data->pod_union.tag = static_cast<internal::PodUnion_Data::PodUnion_Tag>(100);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::SmallStruct_Data::Validate(
      raw_buf, &validation_context));
  free(raw_buf);
}

// Validation fails due to non-nullable null union in struct.
TEST(UnionTest, Validation_NullUnion_Failure) {
  SmallStructNonNullableUnionPtr small_struct(
      SmallStructNonNullableUnion::New());

  size_t size =
      mojo::internal::PrepareToSerialize<SmallStructNonNullableUnionDataView>(
          small_struct, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStructNonNullableUnion_Data* data =
      internal::SmallStructNonNullableUnion_Data::New(&buf);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::SmallStructNonNullableUnion_Data::Validate(
      raw_buf, &validation_context));
  free(raw_buf);
}

// Validation passes with nullable null union.
TEST(UnionTest, Validation_NullableUnion) {
  SmallStructPtr small_struct(SmallStruct::New());

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<SmallStructDataView>(
      small_struct, &context);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::SmallStruct_Data* data = nullptr;
  mojo::internal::Serialize<SmallStructDataView>(small_struct, &buf, &data,
                                                 &context);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::SmallStruct_Data::Validate(
      raw_buf, &validation_context));
  free(raw_buf);
}

// TODO(azani): Move back in map_unittest.cc when possible.
// Map Tests
TEST(UnionTest, PodUnionInMap) {
  SmallStructPtr small_struct(SmallStruct::New());
  small_struct->pod_union_map.emplace();
  small_struct->pod_union_map.value()["one"] = PodUnion::New();
  small_struct->pod_union_map.value()["two"] = PodUnion::New();

  small_struct->pod_union_map.value()["one"]->set_f_int8(8);
  small_struct->pod_union_map.value()["two"]->set_f_int16(16);

  EXPECT_EQ(8, small_struct->pod_union_map.value()["one"]->get_f_int8());
  EXPECT_EQ(16, small_struct->pod_union_map.value()["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerialization) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  std::unordered_map<std::string, PodUnionPtr> map;
  map.insert(std::make_pair("one", PodUnion::New()));
  map.insert(std::make_pair("two", PodUnion::New()));

  map["one"]->set_f_int8(8);
  map["two"]->set_f_int16(16);

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<MojomType>(map, &context);
  EXPECT_EQ(120U, size);

  mojo::internal::FixedBufferForTesting buf(size);

  typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(0, false, nullptr),
      new mojo::internal::ContainerValidateParams(0, false, nullptr));
  mojo::internal::Serialize<MojomType>(map, &buf, &data, &validate_params,
                                       &context);

  std::unordered_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(data, &map2, &context);

  EXPECT_EQ(8, map2["one"]->get_f_int8());
  EXPECT_EQ(16, map2["two"]->get_f_int16());
}

TEST(UnionTest, PodUnionInMapSerializationWithNull) {
  using MojomType = MapDataView<StringDataView, PodUnionDataView>;

  std::unordered_map<std::string, PodUnionPtr> map;
  map.insert(std::make_pair("one", PodUnion::New()));
  map.insert(std::make_pair("two", nullptr));

  map["one"]->set_f_int8(8);

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<MojomType>(map, &context);
  EXPECT_EQ(120U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  typename mojo::internal::MojomTypeTraits<MojomType>::Data* data;
  mojo::internal::ContainerValidateParams validate_params(
      new mojo::internal::ContainerValidateParams(0, false, nullptr),
      new mojo::internal::ContainerValidateParams(0, true, nullptr));
  mojo::internal::Serialize<MojomType>(map, &buf, &data, &validate_params,
                                       &context);

  std::unordered_map<std::string, PodUnionPtr> map2;
  mojo::internal::Deserialize<MojomType>(data, &map2, &context);

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

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(8, obj2->get_f_dummy()->f_int8);
}

TEST(UnionTest, StructInUnionValidation) {
  DummyStructPtr dummy(DummyStruct::New());
  dummy->f_int8 = 8;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, StructInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_dummy(std::move(dummy));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, StructInUnionValidationNullable) {
  DummyStructPtr dummy(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_nullable(std::move(dummy));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, ArrayInUnionGetterSetter) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  EXPECT_EQ(8, obj->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionSerialization) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);

  EXPECT_EQ(8, obj2->get_f_array_int8()[0]);
  EXPECT_EQ(9, obj2->get_f_array_int8()[1]);
}

TEST(UnionTest, ArrayInUnionValidation) {
  std::vector<int8_t> array(2);
  array[0] = 8;
  array[1] = 9;

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_array_int8(std::move(array));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);
  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);

  EXPECT_TRUE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, MapInUnionGetterSetter) {
  std::unordered_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  EXPECT_EQ(1, obj->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionSerialization) {
  std::unordered_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, &context);
  EXPECT_EQ(112U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 &context);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, &context);

  EXPECT_EQ(1, obj2->get_f_map_int8()["one"]);
  EXPECT_EQ(2, obj2->get_f_map_int8()["two"]);
}

TEST(UnionTest, MapInUnionValidation) {
  std::unordered_map<std::string, int8_t> map;
  map.insert({"one", 1});
  map.insert({"two", 2});

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_map_int8(std::move(map));

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, &context);
  EXPECT_EQ(112U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 &context);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);

  EXPECT_TRUE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
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

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  ObjectUnionPtr obj2;
  mojo::internal::Deserialize<ObjectUnionDataView>(data, &obj2, nullptr);
  EXPECT_EQ(10, obj2->get_f_pod_union()->get_f_int8());
}

TEST(UnionTest, UnionInUnionValidation) {
  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int8(10);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);
  EXPECT_EQ(32U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_TRUE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, UnionInUnionValidationNonNullable) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  PodUnionPtr pod(nullptr);

  ObjectUnionPtr obj(ObjectUnion::New());
  obj->set_f_pod_union(std::move(pod));

  size_t size = mojo::internal::PrepareToSerialize<ObjectUnionDataView>(
      obj, false, nullptr);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::ObjectUnion_Data* data = nullptr;
  mojo::internal::Serialize<ObjectUnionDataView>(obj, &buf, &data, false,
                                                 nullptr);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 0, 0);
  EXPECT_FALSE(internal::ObjectUnion_Data::Validate(
      raw_buf, &validation_context, false));
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
  size_t size = mojo::internal::PrepareToSerialize<HandleUnionDataView>(
      handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  mojo::internal::Serialize<HandleUnionDataView>(handle, &buf, &data, false,
                                                 &context);
  EXPECT_EQ(1U, context.handles.size());

  HandleUnionPtr handle2(HandleUnion::New());
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &context);

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
  size_t size = mojo::internal::PrepareToSerialize<HandleUnionDataView>(
      handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  mojo::internal::Serialize<HandleUnionDataView>(handle, &buf, &data, false,
                                                 &context);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 1, 0);
  EXPECT_TRUE(internal::HandleUnion_Data::Validate(
      raw_buf, &validation_context, false));
  free(raw_buf);
}

TEST(UnionTest, HandleInUnionValidationNull) {
  mojo::internal::SerializationWarningObserverForTesting suppress_warning;

  ScopedMessagePipeHandle pipe;
  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_message_pipe(std::move(pipe));

  mojo::internal::SerializationContext context;
  size_t size = mojo::internal::PrepareToSerialize<HandleUnionDataView>(
      handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  mojo::internal::Serialize<HandleUnionDataView>(handle, &buf, &data, false,
                                                 &context);

  void* raw_buf = buf.Leak();
  mojo::internal::ValidationContext validation_context(
      data, static_cast<uint32_t>(size), 1, 0);
  EXPECT_FALSE(internal::HandleUnion_Data::Validate(
      raw_buf, &validation_context, false));
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
  Binding<SmallCache> bindings(&impl, MakeRequest(&ptr));

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
  Binding<SmallCache> bindings(&impl, MakeRequest(&ptr));

  mojo::internal::SerializationContext context;
  HandleUnionPtr handle(HandleUnion::New());
  handle->set_f_small_cache(std::move(ptr));
  size_t size = mojo::internal::PrepareToSerialize<HandleUnionDataView>(
      handle, false, &context);
  EXPECT_EQ(16U, size);

  mojo::internal::FixedBufferForTesting buf(size);
  internal::HandleUnion_Data* data = nullptr;
  mojo::internal::Serialize<HandleUnionDataView>(handle, &buf, &data, false,
                                                 &context);
  EXPECT_EQ(1U, context.handles.size());

  HandleUnionPtr handle2(HandleUnion::New());
  mojo::internal::Deserialize<HandleUnionDataView>(data, &handle2, &context);

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

void ExpectInt16(int16_t value, PodUnionPtr out) {
  EXPECT_EQ(value, out->get_f_int16());
}

TEST(UnionTest, UnionInInterface) {
  base::MessageLoop message_loop;
  UnionInterfaceImpl impl;
  UnionInterfacePtr ptr;
  Binding<UnionInterface> bindings(&impl, MakeRequest(&ptr));

  PodUnionPtr pod(PodUnion::New());
  pod->set_f_int16(16);

  ptr->Echo(std::move(pod), base::Bind(&ExpectInt16, 16));
  base::RunLoop().RunUntilIdle();
}

}  // namespace test
}  // namespace mojo
