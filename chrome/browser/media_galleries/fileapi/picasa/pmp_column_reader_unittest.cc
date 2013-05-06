// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_column_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using picasa::PmpColumnReader;
using picasa::PmpTestHelper;

// Overridden version of Read method to make test code templatable.
bool DoRead(const PmpColumnReader* reader, uint32 row, std::string* target) {
  return reader->ReadString(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint32* target) {
  return reader->ReadUInt32(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, double* target) {
  return reader->ReadDouble64(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint8* target) {
  return reader->ReadUInt8(row, target);
}

bool DoRead(const PmpColumnReader* reader, uint32 row, uint64* target) {
  return reader->ReadUInt64(row, target);
}

// TestValid
template<class T>
void TestValid(const picasa::PmpFieldType field_type,
               const std::vector<T>& elems) {
  PmpTestHelper test_helper("test");
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader;

  uint32 rows_read = 0xFF;

  std::vector<uint8> data =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  ASSERT_TRUE(test_helper.InitColumnReaderFromBytes(
      &reader, data, field_type, &rows_read));
  EXPECT_EQ(elems.size(), rows_read);

  for (uint32 i = 0; i < elems.size() && i < rows_read; i++) {
    T target;
    EXPECT_TRUE(DoRead(&reader, i, &target));
    EXPECT_EQ(elems[i], target);
  }
}

template<class T>
void TestMalformed(const picasa::PmpFieldType field_type,
                   const std::vector<T>& elems) {
  PmpTestHelper test_helper("test");
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader_too_few_declared_rows;
  std::vector<uint8> data_too_few_declared_rows =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size()-1, elems);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_too_few_declared_rows,
      data_too_few_declared_rows,
      field_type,
      NULL));

  PmpColumnReader reader_too_many_declared_rows;
  std::vector<uint8> data_too_many_declared_rows =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size()+1, elems);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_too_many_declared_rows,
      data_too_many_declared_rows,
      field_type,
      NULL));

  PmpColumnReader reader_truncated;
  std::vector<uint8> data_truncated =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  data_truncated.resize(data_truncated.size()-10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_truncated, data_truncated, field_type, NULL));

  PmpColumnReader reader_padded;
  std::vector<uint8> data_padded =
      PmpTestHelper::MakeHeaderAndBody(field_type, elems.size(), elems);
  data_padded.resize(data_padded.size()+10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_padded, data_padded, field_type, NULL));
}

template<class T>
void TestPrimitive(const picasa::PmpFieldType field_type) {
  // Make an ascending vector of the primitive.
  uint32 n = 100;
  std::vector<T> data(n, 0);
  for (uint32 i = 0; i < n; i++) {
    data[i] = i*3;
  }

  TestValid<T>(field_type, data);
  TestMalformed<T>(field_type, data);
}


TEST(PmpColumnReaderTest, HeaderParsingAndValidation) {
  PmpTestHelper test_helper("test");
  ASSERT_TRUE(test_helper.Init());

  PmpColumnReader reader_good_header;
  uint32 rows_read = 0xFF;
  std::vector<uint8> good_header =
      PmpTestHelper::MakeHeader(picasa::PMP_TYPE_STRING, 0);
  EXPECT_TRUE(test_helper.InitColumnReaderFromBytes(
      &reader_good_header,
      good_header,
      picasa::PMP_TYPE_STRING,
      &rows_read));
  EXPECT_EQ(0U, rows_read) << "Read non-zero rows from header-only data.";

  PmpColumnReader reader_bad_magic_bytes;
  std::vector<uint8> bad_magic_bytes =
      PmpTestHelper::MakeHeader(picasa::PMP_TYPE_STRING, 0);
  bad_magic_bytes[0] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_bad_magic_bytes,
      bad_magic_bytes,
      picasa::PMP_TYPE_STRING,
      NULL));

  PmpColumnReader reader_inconsistent_types;
  std::vector<uint8> inconsistent_type =
      PmpTestHelper::MakeHeader(picasa::PMP_TYPE_STRING, 0);
  inconsistent_type[picasa::kPmpFieldType1Offset] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_inconsistent_types,
      inconsistent_type,
      picasa::PMP_TYPE_STRING,
      NULL));

  PmpColumnReader reader_invalid_type;
  std::vector<uint8> invalid_type =
      PmpTestHelper::MakeHeader(picasa::PMP_TYPE_STRING, 0);
  invalid_type[picasa::kPmpFieldType1Offset] = 0xff;
  invalid_type[picasa::kPmpFieldType2Offset] = 0xff;
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_invalid_type,
      invalid_type,
      picasa::PMP_TYPE_STRING,
      NULL));

  PmpColumnReader reader_incomplete_header;
  std::vector<uint8> incomplete_header =
      PmpTestHelper::MakeHeader(picasa::PMP_TYPE_STRING, 0);
  incomplete_header.resize(10);
  EXPECT_FALSE(test_helper.InitColumnReaderFromBytes(
      &reader_incomplete_header,
      incomplete_header,
      picasa::PMP_TYPE_STRING,
      NULL));
}

TEST(PmpColumnReaderTest, StringParsing) {
  std::vector<std::string> empty_strings(100, "");

  // Test empty strings read okay.
  TestValid(picasa::PMP_TYPE_STRING, empty_strings);

  std::vector<std::string> mixed_strings;
  mixed_strings.push_back("");
  mixed_strings.push_back("Hello");
  mixed_strings.push_back("World");
  mixed_strings.push_back("");
  mixed_strings.push_back("123123");
  mixed_strings.push_back("Q");
  mixed_strings.push_back("");

  // Test that a mixed set of strings read correctly.
  TestValid(picasa::PMP_TYPE_STRING, mixed_strings);

  // Test with the data messed up in a variety of ways.
  TestMalformed(picasa::PMP_TYPE_STRING, mixed_strings);
}

TEST(PmpColumnReaderTest, PrimitiveParsing) {
  TestPrimitive<uint32>(picasa::PMP_TYPE_UINT32);
  TestPrimitive<double>(picasa::PMP_TYPE_DOUBLE64);
  TestPrimitive<uint8>(picasa::PMP_TYPE_UINT8);
  TestPrimitive<uint64>(picasa::PMP_TYPE_UINT64);
}

}  // namespace
