// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/cbor/cbor_writer.h"

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
    std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(test_case.value));
    EXPECT_THAT(cbor, testing::ElementsAreArray(test_case.cbor));
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
    std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(test_case.bytes));
    EXPECT_THAT(cbor, testing::ElementsAreArray(test_case.cbor));
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
    std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(test_case.string));
    EXPECT_THAT(cbor, testing::ElementsAreArray(test_case.cbor));
  }
}

TEST(CBORWriterTest, TestWriteArray) {
  static const uint8_t kArrayTestCaseCbor[] = {
      0x98, 0x19, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
      0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12,
      0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x18, 0x18, 0x19};
  std::vector<CBORValue> array;
  for (int i = 1; i <= 25; i++) {
    array.push_back(CBORValue(i));
  }
  std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(array));
  EXPECT_THAT(cbor, testing::ElementsAreArray(kArrayTestCaseCbor,
                                              arraysize(kArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithMapValue) {
  static const uint8_t kMapTestCaseCbor[] = {
      0xa5, 0x61, 0x61, 0x61, 0x41, 0x61, 0x62, 0x61, 0x42, 0x61, 0x63,
      0x61, 0x43, 0x61, 0x64, 0x61, 0x44, 0x61, 0x65, 0x61, 0x45};
  CBORValue::MapValue map;
  map["a"] = CBORValue("A");
  map["b"] = CBORValue("B");
  map["c"] = CBORValue("C");
  map["d"] = CBORValue("D");
  map["e"] = CBORValue("E");
  std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(map));
  EXPECT_THAT(cbor, testing::ElementsAreArray(kMapTestCaseCbor,
                                              arraysize(kMapTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteMapWithArray) {
  static const uint8_t kMapArrayTestCaseCbor[] = {0xa2, 0x61, 0x61, 0x01, 0x61,
                                                  0x62, 0x82, 0x02, 0x03};
  CBORValue::MapValue map;
  map["a"] = CBORValue(1);
  CBORValue::ArrayValue array;
  array.push_back(CBORValue(2));
  array.push_back(CBORValue(3));
  map["b"] = CBORValue(array);
  std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(map));
  EXPECT_THAT(cbor,
              testing::ElementsAreArray(kMapArrayTestCaseCbor,
                                        arraysize(kMapArrayTestCaseCbor)));
}

TEST(CBORWriterTest, TestWriteNestedMap) {
  static const uint8_t kNestedMapTestCase[] = {0xa2, 0x61, 0x61, 0x01, 0x61,
                                               0x62, 0xa2, 0x61, 0x63, 0x02,
                                               0x61, 0x64, 0x03};
  CBORValue::MapValue map;
  map["a"] = CBORValue(1);
  CBORValue::MapValue nested_map;
  nested_map["c"] = CBORValue(2);
  nested_map["d"] = CBORValue(3);
  map["b"] = CBORValue(nested_map);
  std::vector<uint8_t> cbor = CBORWriter::Write(CBORValue(map));
  EXPECT_THAT(cbor, testing::ElementsAreArray(kNestedMapTestCase,
                                              arraysize(kNestedMapTestCase)));
}

}  // namespace content
