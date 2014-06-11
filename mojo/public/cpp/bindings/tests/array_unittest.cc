// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/lib/array_serialization.h"
#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/bindings/tests/sample_service.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class CopyableType {
 public:
  CopyableType() : copied_(false), ptr_(this) { num_instances_++; }
  CopyableType(const CopyableType& other) : copied_(true), ptr_(other.ptr()) {
    num_instances_++;
  }
  CopyableType& operator=(const CopyableType& other) {
    copied_ = true;
    ptr_ = other.ptr();
    return *this;
  }
  ~CopyableType() { num_instances_--; }

  bool copied() const { return copied_; }
  static size_t num_instances() { return num_instances_; }
  CopyableType* ptr() const { return ptr_; }
  void ResetCopied() { copied_ = false; }

 private:
  bool copied_;
  static size_t num_instances_;
  CopyableType* ptr_;
};

size_t CopyableType::num_instances_ = 0;

class MoveOnlyType {
  MOJO_MOVE_ONLY_TYPE_FOR_CPP_03(MoveOnlyType, RValue)
 public:
  typedef MoveOnlyType Data_;
  MoveOnlyType() : moved_(false), ptr_(this) { num_instances_++; }
  MoveOnlyType(RValue other) : moved_(true), ptr_(other.object->ptr()) {
    num_instances_++;
  }
  MoveOnlyType& operator=(RValue other) {
    moved_ = true;
    ptr_ = other.object->ptr();
    return *this;
  }
  ~MoveOnlyType() { num_instances_--; }

  bool moved() const { return moved_; }
  static size_t num_instances() { return num_instances_; }
  MoveOnlyType* ptr() const { return ptr_; }
  void ResetMoved() { moved_ = false; }

 private:
  bool moved_;
  static size_t num_instances_;
  MoveOnlyType* ptr_;
};

size_t MoveOnlyType::num_instances_ = 0;

// Tests that basic Array operations work.
TEST(ArrayTest, Basic) {
  Array<char> array(8);
  for (size_t i = 0; i < array.size(); ++i) {
    char val = static_cast<char>(i*2);
    array[i] = val;
    EXPECT_EQ(val, array.at(i));
  }
}

// Tests that basic Array<bool> operations work.
TEST(ArrayTest, Bool) {
  Array<bool> array(64);
  for (size_t i = 0; i < array.size(); ++i) {
    bool val = i % 3 == 0;
    array[i] = val;
    EXPECT_EQ(val, array.at(i));
  }
}

// Tests that Array<ScopedMessagePipeHandle> supports transferring handles.
TEST(ArrayTest, Handle) {
  MessagePipe pipe;
  Array<ScopedMessagePipeHandle> handles(2);
  handles[0] = pipe.handle0.Pass();
  handles[1].reset(pipe.handle1.release());

  EXPECT_FALSE(pipe.handle0.is_valid());
  EXPECT_FALSE(pipe.handle1.is_valid());

  Array<ScopedMessagePipeHandle> handles2 = handles.Pass();
  EXPECT_TRUE(handles2[0].is_valid());
  EXPECT_TRUE(handles2[1].is_valid());

  ScopedMessagePipeHandle pipe_handle = handles2[0].Pass();
  EXPECT_TRUE(pipe_handle.is_valid());
  EXPECT_FALSE(handles2[0].is_valid());
}

// Tests that Array<ScopedMessagePipeHandle> supports closing handles.
TEST(ArrayTest, HandlesAreClosed) {
  MessagePipe pipe;
  MojoHandle pipe0_value = pipe.handle0.get().value();
  MojoHandle pipe1_value = pipe.handle0.get().value();

  {
    Array<ScopedMessagePipeHandle> handles(2);
    handles[0] = pipe.handle0.Pass();
    handles[1].reset(pipe.handle0.release());
  }

  // We expect the pipes to have been closed.
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe0_value));
  EXPECT_EQ(MOJO_RESULT_INVALID_ARGUMENT, MojoClose(pipe1_value));
}

TEST(ArrayTest, Serialization_ArrayOfPOD) {
  Array<int32_t> array(4);
  for (size_t i = 0; i < array.size(); ++i)
    array[i] = static_cast<int32_t>(i);

  size_t size = GetSerializedSize_(array);
  EXPECT_EQ(8U + 4*4U, size);

  internal::FixedBuffer buf(size);
  internal::Array_Data<int32_t>* data;
  Serialize_(array.Pass(), &buf, &data);

  Array<int32_t> array2;
  Deserialize_(data, &array2);

  EXPECT_EQ(4U, array2.size());
  for (size_t i = 0; i < array2.size(); ++i)
    EXPECT_EQ(static_cast<int32_t>(i), array2[i]);
}

TEST(ArrayTest, Serialization_ArrayOfArrayOfPOD) {
  Array<Array<int32_t> > array(2);
  for (size_t j = 0; j < array.size(); ++j) {
    Array<int32_t> inner(4);
    for (size_t i = 0; i < inner.size(); ++i)
      inner[i] = static_cast<int32_t>(i + (j * 10));
    array[j] = inner.Pass();
  }

  size_t size = GetSerializedSize_(array);
  EXPECT_EQ(8U + 2*8U + 2*(8U + 4*4U), size);

  internal::FixedBuffer buf(size);
  internal::Array_Data<internal::Array_Data<int32_t>*>* data;
  Serialize_(array.Pass(), &buf, &data);

  Array<Array<int32_t> > array2;
  Deserialize_(data, &array2);

  EXPECT_EQ(2U, array2.size());
  for (size_t j = 0; j < array2.size(); ++j) {
    const Array<int32_t>& inner = array2[j];
    EXPECT_EQ(4U, inner.size());
    for (size_t i = 0; i < inner.size(); ++i)
      EXPECT_EQ(static_cast<int32_t>(i + (j * 10)), inner[i]);
  }
}

TEST(ArrayTest, Serialization_ArrayOfBool) {
  Array<bool> array(10);
  for (size_t i = 0; i < array.size(); ++i)
    array[i] = i % 2 ? true : false;

  size_t size = GetSerializedSize_(array);
  EXPECT_EQ(8U + 8U, size);

  internal::FixedBuffer buf(size);
  internal::Array_Data<bool>* data;
  Serialize_(array.Pass(), &buf, &data);

  Array<bool> array2;
  Deserialize_(data, &array2);

  EXPECT_EQ(10U, array2.size());
  for (size_t i = 0; i < array2.size(); ++i)
    EXPECT_EQ(i % 2 ? true : false, array2[i]);
}

TEST(ArrayTest, Serialization_ArrayOfString) {
  Array<String> array(10);
  for (size_t i = 0; i < array.size(); ++i) {
    char c = 'A' + 1;
    array[i] = String(&c, 1);
  }

  size_t size = GetSerializedSize_(array);
  EXPECT_EQ(8U +     // array header
            10*8U +  // array payload (10 pointers)
            10*(8U +  // string header
                8U),  // string length of 1 padded to 8
            size);

  internal::FixedBuffer buf(size);
  internal::Array_Data<internal::String_Data*>* data;
  Serialize_(array.Pass(), &buf, &data);

  Array<String> array2;
  Deserialize_(data, &array2);

  EXPECT_EQ(10U, array2.size());
  for (size_t i = 0; i < array2.size(); ++i) {
    char c = 'A' + 1;
    EXPECT_EQ(String(&c, 1), array2[i]);
  }
}

TEST(ArrayTest, Resize_Copyable) {
  ASSERT_EQ(0u, CopyableType::num_instances());
  mojo::Array<CopyableType> array(3);
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
  array.reset();
  EXPECT_EQ(0u, CopyableType::num_instances());
  EXPECT_FALSE(array);
  array.resize(0);
  EXPECT_EQ(0u, CopyableType::num_instances());
  EXPECT_TRUE(array);
}

TEST(ArrayTest, Resize_MoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  mojo::Array<MoveOnlyType> array(3);
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

  array.reset();
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
  EXPECT_FALSE(array);
  array.resize(0);
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
  EXPECT_TRUE(array);
}

TEST(ArrayTest, PushBack_Copyable) {
  ASSERT_EQ(0u, CopyableType::num_instances());
  mojo::Array<CopyableType> array(2);
  array.reset();
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
  array.reset();
  EXPECT_EQ(0u, CopyableType::num_instances());
}

TEST(ArrayTest, PushBack_MoveOnly) {
  ASSERT_EQ(0u, MoveOnlyType::num_instances());
  mojo::Array<MoveOnlyType> array(2);
  array.reset();
  std::vector<MoveOnlyType*> value_ptrs;
  size_t capacity = array.storage().capacity();
  for (size_t i = 0; i < capacity; i++) {
    MoveOnlyType value;
    value_ptrs.push_back(value.ptr());
    array.push_back(value.Pass());
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
    array.push_back(value.Pass());
    EXPECT_EQ(array.size() + 1, MoveOnlyType::num_instances());
  }
  ASSERT_EQ(capacity + 1, array.size());
  EXPECT_EQ(array.size(), MoveOnlyType::num_instances());

  for (size_t i = 0; i < array.size(); i++) {
    EXPECT_TRUE(array[i].moved());
    EXPECT_EQ(value_ptrs[i], array[i].ptr());
  }
  array.reset();
  EXPECT_EQ(0u, MoveOnlyType::num_instances());
}

}  // namespace
}  // namespace test
}  // namespace mojo
