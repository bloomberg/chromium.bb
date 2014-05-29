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
  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  Array<ScopedMessagePipeHandle> handles(2);
  handles[0] = pipe0.Pass();
  handles[1].reset(pipe1.release());

  EXPECT_FALSE(pipe0.is_valid());
  EXPECT_FALSE(pipe1.is_valid());

  Array<ScopedMessagePipeHandle> handles2 = handles.Pass();
  EXPECT_TRUE(handles2[0].is_valid());
  EXPECT_TRUE(handles2[1].is_valid());

  pipe0 = handles2[0].Pass();
  EXPECT_TRUE(pipe0.is_valid());
  EXPECT_FALSE(handles2[0].is_valid());
}

// Tests that Array<ScopedMessagePipeHandle> supports closing handles.
TEST(ArrayTest, HandlesAreClosed) {
  ScopedMessagePipeHandle pipe0, pipe1;
  CreateMessagePipe(&pipe0, &pipe1);

  MojoHandle pipe0_value = pipe0.get().value();
  MojoHandle pipe1_value = pipe1.get().value();

  {
    Array<ScopedMessagePipeHandle> handles(2);
    handles[0] = pipe0.Pass();
    handles[1].reset(pipe1.release());
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

}  // namespace
}  // namespace test
}  // namespace mojo
