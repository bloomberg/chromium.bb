// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/cbor/cbor_writer.h"

#include <string>

#include "base/strings/string_piece.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

/* Leveraging RFC 7049 examples from
   https://github.com/cbor/test-vectors/blob/master/appendix_a.json. */
namespace content {

TEST(CBORWriterTest, TestWriteUint) {
  typedef struct {
    const uint64_t value;
    const base::StringPiece cbor;
  } UintTestCase;

  static const UintTestCase kUintTestCases[] = {
      // Reminder: must specify length when creating string pieces
      // with null bytes, else the string will truncate prematurely.
      {0, base::StringPiece("\x00", 1)},
      {1, base::StringPiece("\x01")},
      {10, base::StringPiece("\x0a")},
      {23, base::StringPiece("\x17")},
      {24, base::StringPiece("\x18\x18")},
      {25, base::StringPiece("\x18\x19")},
      {100, base::StringPiece("\x18\x64")},
      {1000, base::StringPiece("\x19\x03\xe8")},
      {1000000, base::StringPiece("\x1a\x00\x0f\x42\x40", 5)},
      {0xFFFFFFFF, base::StringPiece("\x1a\xff\xff\xff\xff")},
  };

  for (const UintTestCase& test_case : kUintTestCases) {
    auto cbor = CBORWriter::Write(CBORValue(test_case.value));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST(CBORWriterTest, TestWriteBytes) {
  typedef struct {
    const std::vector<uint8_t> bytes;
    const base::StringPiece cbor;
  } BytesTestCase;

  static const BytesTestCase kBytesTestCases[] = {
      {{}, base::StringPiece("\x40")},
      {{0x01, 0x02, 0x03, 0x04}, base::StringPiece("\x44\x01\x02\x03\x04")},
  };

  for (const BytesTestCase& test_case : kBytesTestCases) {
    auto cbor = CBORWriter::Write(CBORValue(test_case.bytes));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST(CBORWriterTest, TestWriteString) {
  typedef struct {
    const std::string string;
    const base::StringPiece cbor;
  } StringTestCase;

  static const StringTestCase kStringTestCases[] = {
      {"", base::StringPiece("\x60")},
      {"a", base::StringPiece("\x61\x61")},
      {"IETF", base::StringPiece("\x64\x49\x45\x54\x46")},
      {"\"\\", base::StringPiece("\x62\x22\x5c")},
      {"\xc3\xbc", base::StringPiece("\x62\xc3\xbc")},
      {"\xe6\xb0\xb4", base::StringPiece("\x63\xe6\xb0\xb4")},
      {"\xf0\x90\x85\x91", base::StringPiece("\x64\xf0\x90\x85\x91")}};

  for (const StringTestCase& test_case : kStringTestCases) {
    auto cbor = CBORWriter::Write(CBORValue(test_case.string));
    ASSERT_TRUE(cbor.has_value());
    EXPECT_THAT(cbor.value(), testing::ElementsAreArray(test_case.cbor));
  }
}

TEST(CBORWriterTest, TestWriteArray) {
  static const uint8_t kArrayTestCaseCbor[] = {
      // clang-format off
      0x98, 0x19,  // array of 25 elements
        0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c,
        0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
        0x18, 0x18, 0x19,
      // clang-format on
  };
  std::vector<CBORValue> array;
  for (int i = 1; i <= 25; i++) {
    array.push_back(CBORValue(i));
  }
  auto cbor = CBORWriter::Write(CBORValue(array));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kArrayTestCaseCbor,
                                        arraysize(kArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithMapValue) {
  static const uint8_t kMapTestCaseCbor[] = {
      // clang-format off
      0xAF,  // map of 7 pairs:
        0x00,          // key 0
        0x61, 0x61,    // value "a"

        0x17,          // key 23
        0x61,  0x62,   // value "b"

        0x18, 0x18,    // key 24
        0x61,   0x63,  // value "c"

        0x18, 0xFF,        // key 255
        0x61,  0x64,       // value "d"

        0x19, 0x01, 0x00,  // key 256
        0x61, 0x65,        // value "e"

        0x19, 0xFF, 0xFF,  // key 65535
        0x61,  0x66,       // value "f"

        0x1A, 0x00, 0x01, 0x00, 0x00,   // key 65536
        0x61, 0x67,                     // value "g"

        0x1A, 0xFF, 0xFF, 0xFF, 0xFF,   // key 4294967295
        0x61, 0x68,                     // value "h"

        // key 4294967296
        0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
        0x61, 0x69,  //  value "i"

        0x60,        // key ""
        0x61, 0x2e,  // value "."

        0x61, 0x62,  // key "b"
        0x61, 0x42,  // value "B"

        0x61, 0x63,  // key "c"
        0x61, 0x43,  // value "C"

        0x61, 0x64,  // key "d"
        0x61, 0x44,  // value "D"

        0x61, 0x65,  // key "e"
        0x61, 0x45,  // value "E"

        0x62, 0x61, 0x61,  // key "aa"
        0x62, 0x41, 0x41,  // value "AA"
      // clang-format on
  };
  CBORValue::MapValue map;
  // Shorter strings sort first in CTAP, thus the “aa” value should be
  // serialised last in the map.
  map[CBORValue("aa")] = CBORValue("AA");
  map[CBORValue("d")] = CBORValue("D");
  map[CBORValue("b")] = CBORValue("B");
  map[CBORValue("e")] = CBORValue("E");
  map[CBORValue("c")] = CBORValue("C");
  // The empty string is shorter than all others, so should appear first among
  // the strings.
  map[CBORValue("")] = CBORValue(".");
  // Map keys are sorted by major type, by byte length, and then by
  // byte-wise lexical order. So all integer type keys should appear before
  // key "".
  map[CBORValue(uint64_t(0))] = CBORValue("a");
  map[CBORValue(23)] = CBORValue("b");
  map[CBORValue(24)] = CBORValue("c");
  map[CBORValue(255)] = CBORValue("d");
  map[CBORValue(256)] = CBORValue("e");
  map[CBORValue(65535)] = CBORValue("f");
  map[CBORValue(65536)] = CBORValue("g");
  map[CBORValue(4294967295)] = CBORValue("h");
  map[CBORValue(4294967296)] = CBORValue("i");

  auto cbor = CBORWriter::Write(CBORValue(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(), testing::ElementsAreArray(
                                kMapTestCaseCbor, arraysize(kMapTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithArray) {
  static const uint8_t kMapArrayTestCaseCbor[] = {
      // clang-format off
      0xa2,  // map of 2 pairs
        0x61, 0x61,  // "a"
        0x01,

        0x61, 0x62,  // "b"
        0x82,        // array with 2 elements
          0x02,
          0x03,
      // clang-format on
  };
  CBORValue::MapValue map;
  map[CBORValue("a")] = CBORValue(1);
  CBORValue::ArrayValue array;
  array.push_back(CBORValue(2));
  array.push_back(CBORValue(3));
  map[CBORValue("b")] = CBORValue(array);
  auto cbor = CBORWriter::Write(CBORValue(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kMapArrayTestCaseCbor,
                                        arraysize(kMapArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteNestedMap) {
  static const uint8_t kNestedMapTestCase[] = {
      // clang-format off
      0xa2,  // map of 2 pairs
        0x61, 0x61,  // "a"
        0x01,

        0x61, 0x62,  // "b"
        0xa2,        // map of 2 pairs
          0x61, 0x63,  // "c"
          0x02,

          0x61, 0x64,  // "d"
          0x03,
      // clang-format on
  };
  CBORValue::MapValue map;
  map[CBORValue("a")] = CBORValue(1);
  CBORValue::MapValue nested_map;
  nested_map[CBORValue("c")] = CBORValue(2);
  nested_map[CBORValue("d")] = CBORValue(3);
  map[CBORValue("b")] = CBORValue(nested_map);
  auto cbor = CBORWriter::Write(CBORValue(map));
  ASSERT_TRUE(cbor.has_value());
  EXPECT_THAT(cbor.value(),
              testing::ElementsAreArray(kNestedMapTestCase,
                                        arraysize(kNestedMapTestCase)));
}

// For major type 0, 2, 3, empty CBOR array, and empty CBOR map, the nesting
// depth is expected to be 0 since the CBOR decoder does not need to parse
// any nested CBOR value elements.
TEST(CBORWriterTest, TestWriteSingleLayer) {
  const CBORValue simple_uint = CBORValue(1);
  const CBORValue simple_string = CBORValue("a");
  const std::vector<uint8_t> byte_data = {0x01, 0x02, 0x03, 0x04};
  const CBORValue simple_bytestring = CBORValue(byte_data);
  CBORValue::ArrayValue empty_cbor_array;
  CBORValue::MapValue empty_cbor_map;
  const CBORValue empty_array_value = CBORValue(empty_cbor_array);
  const CBORValue empty_map_value = CBORValue(empty_cbor_map);
  CBORValue::ArrayValue simple_array;
  simple_array.push_back(CBORValue(2));
  CBORValue::MapValue simple_map;
  simple_map[CBORValue("b")] = CBORValue(3);
  const CBORValue single_layer_cbor_map = CBORValue(simple_map);
  const CBORValue single_layer_cbor_array = CBORValue(simple_array);

  EXPECT_TRUE(CBORWriter::Write(simple_uint, 0).has_value());
  EXPECT_TRUE(CBORWriter::Write(simple_string, 0).has_value());
  EXPECT_TRUE(CBORWriter::Write(simple_bytestring, 0).has_value());

  EXPECT_TRUE(CBORWriter::Write(empty_array_value, 0).has_value());
  EXPECT_TRUE(CBORWriter::Write(empty_map_value, 0).has_value());

  EXPECT_FALSE(CBORWriter::Write(single_layer_cbor_array, 0).has_value());
  EXPECT_TRUE(CBORWriter::Write(single_layer_cbor_array, 1).has_value());

  EXPECT_FALSE(CBORWriter::Write(single_layer_cbor_map, 0).has_value());
  EXPECT_TRUE(CBORWriter::Write(single_layer_cbor_map, 1).has_value());
}

// Major type 5 nested CBOR map value with following structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3}}
TEST(CBORWriterTest, NestedMaps) {
  CBORValue::MapValue cbor_map;
  cbor_map[CBORValue("a")] = CBORValue(1);
  CBORValue::MapValue nested_map;
  nested_map[CBORValue("c")] = CBORValue(2);
  nested_map[CBORValue("d")] = CBORValue(3);
  cbor_map[CBORValue("b")] = CBORValue(nested_map);
  EXPECT_TRUE(CBORWriter::Write(CBORValue(cbor_map), 2).has_value());
  EXPECT_FALSE(CBORWriter::Write(CBORValue(cbor_map), 1).has_value());
}

// Testing Write() function for following CBOR structure with depth of 3.
//     [1,
//      2,
//      3,
//      {"a": 1,
//       "b": {"c": 2,
//             "d": 3}}]
TEST(CBORWriterTest, UnbalancedNestedContainers) {
  CBORValue::ArrayValue cbor_array;
  CBORValue::MapValue cbor_map;
  CBORValue::MapValue nested_map;

  cbor_map[CBORValue("a")] = CBORValue(1);
  nested_map[CBORValue("c")] = CBORValue(2);
  nested_map[CBORValue("d")] = CBORValue(3);
  cbor_map[CBORValue("b")] = CBORValue(nested_map);
  cbor_array.push_back(CBORValue(1));
  cbor_array.push_back(CBORValue(2));
  cbor_array.push_back(CBORValue(3));
  cbor_array.push_back(CBORValue(cbor_map));

  EXPECT_TRUE(CBORWriter::Write(CBORValue(cbor_array), 3).has_value());
  EXPECT_FALSE(CBORWriter::Write(CBORValue(cbor_array), 2).has_value());
}

// Testing Write() function for following CBOR structure.
//     {"a": 1,
//      "b": {"c": 2,
//            "d": 3
//            "h": { "e": 4,
//                   "f": 5,
//                   "g": [6, 7, [8]]}}}
// Since above CBOR contains 5 nesting levels. Thus, Write() is expected to
// return empty optional object when maximum nesting layer size is set to 4.
TEST(CBORWriterTest, OverlyNestedCBOR) {
  CBORValue::MapValue map;
  CBORValue::MapValue nested_map;
  CBORValue::MapValue inner_nested_map;
  CBORValue::ArrayValue inner_array;
  CBORValue::ArrayValue array;

  map[CBORValue("a")] = CBORValue(1);
  nested_map[CBORValue("c")] = CBORValue(2);
  nested_map[CBORValue("d")] = CBORValue(3);
  inner_nested_map[CBORValue("e")] = CBORValue(4);
  inner_nested_map[CBORValue("f")] = CBORValue(5);
  inner_array.push_back(CBORValue(6));
  array.push_back(CBORValue(6));
  array.push_back(CBORValue(7));
  array.push_back(CBORValue(inner_array));
  inner_nested_map[CBORValue("g")] = CBORValue(array);
  nested_map[CBORValue("h")] = CBORValue(inner_nested_map);
  map[CBORValue("b")] = CBORValue(nested_map);

  EXPECT_TRUE(CBORWriter::Write(CBORValue(map), 5).has_value());
  EXPECT_FALSE(CBORWriter::Write(CBORValue(map), 4).has_value());
}

}  // namespace content
