// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/macros.h"
#include "blimp/helium/serializable_struct.h"
#include "blimp/helium/stream_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/protobuf/src/google/protobuf/io/coded_stream.h"
#include "third_party/protobuf/src/google/protobuf/io/zero_copy_stream_impl_lite.h"

namespace blimp {
namespace helium {
namespace {

struct EmptyStruct : public SerializableStruct {
  EmptyStruct() {}
  ~EmptyStruct() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(EmptyStruct);
};

struct TwoFieldStruct : public SerializableStruct {
  TwoFieldStruct() : field_1(this), field_2(this) {}
  ~TwoFieldStruct() override {}

  Field<int32_t> field_1;
  Field<int32_t> field_2;

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoFieldStruct);
};

class SerializableStructTest : public testing::Test {
 public:
  SerializableStructTest() {}
  ~SerializableStructTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SerializableStructTest);
};

TEST_F(SerializableStructTest, ByteFormatTwoFieldsSet) {
  TwoFieldStruct test_struct;
  test_struct.field_1.Set(5);
  test_struct.field_2.Set(6);

  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  test_struct.Serialize(&output_stream);
  output_stream.Trim();

  EXPECT_EQ("\x5\x6", changeset);
}

TEST_F(SerializableStructTest, ByteFormatOneFieldSet) {
  TwoFieldStruct test_struct;
  test_struct.field_2.Set(6);

  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  test_struct.Serialize(&output_stream);
  output_stream.Trim();

  EXPECT_EQ(0x00, changeset[0]);
  EXPECT_EQ(0x06, changeset[1]);
}

TEST_F(SerializableStructTest, EmptyStruct) {
  EmptyStruct test_struct;

  std::string changeset;
  google::protobuf::io::StringOutputStream raw_output_stream(&changeset);
  google::protobuf::io::CodedOutputStream output_stream(&raw_output_stream);
  test_struct.Serialize(&output_stream);
  output_stream.Trim();

  EXPECT_TRUE(changeset.empty());
}

}  // namespace
}  // namespace helium
}  // namespace blimp
