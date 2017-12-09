// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/cbor/cbor_values.h"

#include <string>
#include <utility>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(CBORValuesTest, TestNothrow) {
  static_assert(std::is_nothrow_move_constructible<CBORValue>::value,
                "IsNothrowMoveConstructible");
  static_assert(std::is_nothrow_default_constructible<CBORValue>::value,
                "IsNothrowDefaultConstructible");
  static_assert(std::is_nothrow_constructible<CBORValue, std::string&&>::value,
                "IsNothrowMoveConstructibleFromString");
  static_assert(
      std::is_nothrow_constructible<CBORValue, CBORValue::BinaryValue&&>::value,
      "IsNothrowMoveConstructibleFromBytestring");
  static_assert(
      std::is_nothrow_constructible<CBORValue, CBORValue::ArrayValue&&>::value,
      "IsNothrowMoveConstructibleFromArray");
  static_assert(std::is_nothrow_move_assignable<CBORValue>::value,
                "IsNothrowMoveAssignable");
}

// Test constructors
TEST(CBORValuesTest, ConstructUnsigned) {
  CBORValue value(37);
  EXPECT_EQ(CBORValue::Type::UNSIGNED, value.type());
  EXPECT_EQ(37u, value.GetUnsigned());
}

TEST(CBORValuesTest, ConstructStringFromConstCharPtr) {
  const char* str = "foobar";
  CBORValue value(str);
  EXPECT_EQ(CBORValue::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructStringFromStdStringConstRef) {
  std::string str = "foobar";
  CBORValue value(str);
  EXPECT_EQ(CBORValue::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructStringFromStdStringRefRef) {
  std::string str = "foobar";
  CBORValue value(std::move(str));
  EXPECT_EQ(CBORValue::Type::STRING, value.type());
  EXPECT_EQ("foobar", value.GetString());
}

TEST(CBORValuesTest, ConstructBytestring) {
  CBORValue value(CBORValue::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  EXPECT_EQ(CBORValue::Type::BYTE_STRING, value.type());
  EXPECT_EQ(CBORValue::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}),
            value.GetBytestring());
}

TEST(CBORValuesTest, ConstructArray) {
  CBORValue::ArrayValue array;
  array.emplace_back(CBORValue("foo"));
  {
    CBORValue value(array);
    EXPECT_EQ(CBORValue::Type::ARRAY, value.type());
    EXPECT_EQ(1u, value.GetArray().size());
    EXPECT_EQ(CBORValue::Type::STRING, value.GetArray()[0].type());
    EXPECT_EQ("foo", value.GetArray()[0].GetString());
  }

  array.back() = CBORValue("bar");
  {
    CBORValue value(std::move(array));
    EXPECT_EQ(CBORValue::Type::ARRAY, value.type());
    EXPECT_EQ(1u, value.GetArray().size());
    EXPECT_EQ(CBORValue::Type::STRING, value.GetArray()[0].type());
    EXPECT_EQ("bar", value.GetArray()[0].GetString());
  }
}

TEST(CBORValuesTest, ConstructMap) {
  CBORValue::MapValue map;
  map.emplace("foo", CBORValue("bar"));
  {
    CBORValue value(map);
    EXPECT_EQ(CBORValue::Type::MAP, value.type());
    ASSERT_EQ(value.GetMap().count("foo"), 1u);
    EXPECT_EQ(CBORValue::Type::STRING,
              value.GetMap().find("foo")->second.type());
    EXPECT_EQ("bar", value.GetMap().find("foo")->second.GetString());
  }

  map["foo"] = CBORValue("baz");
  {
    CBORValue value(std::move(map));
    EXPECT_EQ(CBORValue::Type::MAP, value.type());
    ASSERT_EQ(value.GetMap().count("foo"), 1u);
    EXPECT_EQ(CBORValue::Type::STRING,
              value.GetMap().find("foo")->second.type());
    EXPECT_EQ("baz", value.GetMap().find("foo")->second.GetString());
  }
}

// Test copy constructors
TEST(CBORValuesTest, CopyUnsigned) {
  CBORValue value(74);
  CBORValue copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetUnsigned(), copied_value.GetUnsigned());

  CBORValue blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetUnsigned(), blank.GetUnsigned());
}

TEST(CBORValuesTest, CopyString) {
  CBORValue value("foobar");
  CBORValue copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetString(), copied_value.GetString());

  CBORValue blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetString(), blank.GetString());
}

TEST(CBORValuesTest, CopyBytestring) {
  CBORValue value(CBORValue::BinaryValue({0xF, 0x0, 0x0, 0xB, 0xA, 0x2}));
  CBORValue copied_value(value.Clone());
  EXPECT_EQ(value.type(), copied_value.type());
  EXPECT_EQ(value.GetBytestring(), copied_value.GetBytestring());

  CBORValue blank;

  blank = value.Clone();
  EXPECT_EQ(value.type(), blank.type());
  EXPECT_EQ(value.GetBytestring(), blank.GetBytestring());
}

TEST(CBORValuesTest, CopyArray) {
  CBORValue::ArrayValue array;
  array.emplace_back(123);
  CBORValue value(std::move(array));

  CBORValue copied_value(value.Clone());
  EXPECT_EQ(1u, copied_value.GetArray().size());
  EXPECT_EQ(value.GetArray()[0].GetUnsigned(),
            copied_value.GetArray()[0].GetUnsigned());

  CBORValue blank;
  blank = value.Clone();
  EXPECT_EQ(1u, blank.GetArray().size());
}

TEST(CBORValuesTest, CopyMap) {
  CBORValue::MapValue map;
  map.emplace("unsigned", CBORValue(123));
  CBORValue value(std::move(map));

  CBORValue copied_value(value.Clone());
  EXPECT_EQ(1u, copied_value.GetMap().size());
  ASSERT_EQ(value.GetMap().count("unsigned"), 1u);
  ASSERT_EQ(copied_value.GetMap().count("unsigned"), 1u);
  EXPECT_EQ(value.GetMap().find("unsigned")->second.GetUnsigned(),
            copied_value.GetMap().find("unsigned")->second.GetUnsigned());

  CBORValue blank;
  blank = value.Clone();
  EXPECT_EQ(1u, blank.GetMap().size());
  ASSERT_EQ(value.GetMap().count("unsigned"), 1u);
  ASSERT_EQ(copied_value.GetMap().count("unsigned"), 1u);
  EXPECT_EQ(value.GetMap().find("unsigned")->second.GetUnsigned(),
            copied_value.GetMap().find("unsigned")->second.GetUnsigned());
}

// Test move constructors and move-assignment
TEST(CBORValuesTest, MoveUnsigned) {
  CBORValue value(74);
  CBORValue moved_value(std::move(value));
  EXPECT_EQ(CBORValue::Type::UNSIGNED, moved_value.type());
  EXPECT_EQ(74u, moved_value.GetUnsigned());

  CBORValue blank;

  blank = CBORValue(654);
  EXPECT_EQ(CBORValue::Type::UNSIGNED, blank.type());
  EXPECT_EQ(654u, blank.GetUnsigned());
}

TEST(CBORValuesTest, MoveString) {
  CBORValue value("foobar");
  CBORValue moved_value(std::move(value));
  EXPECT_EQ(CBORValue::Type::STRING, moved_value.type());
  EXPECT_EQ("foobar", moved_value.GetString());

  CBORValue blank;

  blank = CBORValue("foobar");
  EXPECT_EQ(CBORValue::Type::STRING, blank.type());
  EXPECT_EQ("foobar", blank.GetString());
}

TEST(CBORValuesTest, MoveBytestring) {
  const CBORValue::BinaryValue bytes({0xF, 0x0, 0x0, 0xB, 0xA, 0x2});
  CBORValue value(bytes);
  CBORValue moved_value(std::move(value));
  EXPECT_EQ(CBORValue::Type::BYTE_STRING, moved_value.type());
  EXPECT_EQ(bytes, moved_value.GetBytestring());

  CBORValue blank;

  blank = CBORValue(bytes);
  EXPECT_EQ(CBORValue::Type::BYTE_STRING, blank.type());
  EXPECT_EQ(bytes, blank.GetBytestring());
}

TEST(CBORValuesTest, MoveConstructMap) {
  CBORValue::MapValue map;
  map.emplace("unsigned", CBORValue(123));

  CBORValue value(std::move(map));
  CBORValue moved_value(std::move(value));
  EXPECT_EQ(CBORValue::Type::MAP, moved_value.type());
  ASSERT_EQ(moved_value.GetMap().count("unsigned"), 1u);
  EXPECT_EQ(123u, moved_value.GetMap().find("unsigned")->second.GetUnsigned());
}

TEST(CBORValuesTest, MoveAssignMap) {
  CBORValue::MapValue map;
  map.emplace("unsigned", CBORValue(123));

  CBORValue blank;
  blank = CBORValue(std::move(map));
  EXPECT_EQ(CBORValue::Type::MAP, blank.type());
  ASSERT_EQ(blank.GetMap().count("unsigned"), 1u);
  EXPECT_EQ(123u, blank.GetMap().find("unsigned")->second.GetUnsigned());
}

TEST(CBORValuesTest, MoveArray) {
  CBORValue::ArrayValue array;
  array.emplace_back(123);
  CBORValue value(array);
  CBORValue moved_value(std::move(value));
  EXPECT_EQ(CBORValue::Type::ARRAY, moved_value.type());
  EXPECT_EQ(123u, moved_value.GetArray().back().GetUnsigned());

  CBORValue blank;
  blank = CBORValue(std::move(array));
  EXPECT_EQ(CBORValue::Type::ARRAY, blank.type());
  EXPECT_EQ(123u, blank.GetArray().back().GetUnsigned());
}

TEST(CBORValuesTest, SelfSwap) {
  CBORValue test(1);
  std::swap(test, test);
  EXPECT_TRUE(test.GetUnsigned() == 1);
}

}  // namespace content
