// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_column_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_constants.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_table_reader.h"
#include "chrome/browser/media_galleries/fileapi/picasa/pmp_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using picasa::PmpTestHelper;

TEST(PmpTableReaderTest, RowCountAndFieldType) {
  std::string table_name("tabletest");
  PmpTestHelper test_helper(table_name);
  ASSERT_TRUE(test_helper.Init());

  std::vector<std::string> column_names;
  column_names.push_back("strings");
  column_names.push_back("uint32s");
  column_names.push_back("doubles");

  const std::vector<std::string> strings_vector(10, "Hello");
  const std::vector<uint32> uint32s_vector(30, 42);
  const std::vector<double> doubles_vector(20, 0.5);

  picasa::PmpFieldType column_field_types[] = {
    picasa::PMP_TYPE_STRING,
    picasa::PMP_TYPE_UINT32,
    picasa::PMP_TYPE_DOUBLE64
  };

  const uint32 max_rows = uint32s_vector.size();

  // Write three column files, one each for strings, uint32s, and doubles.

  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      column_names[0], column_field_types[0], strings_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      column_names[1], column_field_types[1], uint32s_vector));
  ASSERT_TRUE(test_helper.WriteColumnFileFromVector(
      column_names[2], column_field_types[2], doubles_vector));

  picasa::PmpTableReader table_reader(table_name,
                                            test_helper.GetTempDirPath());

  for (unsigned int i = 0; i < column_names.size(); i++) {
    ASSERT_TRUE(
        table_reader.AddColumn(column_names[i], column_field_types[i]) != NULL);
  }

  EXPECT_EQ(max_rows, table_reader.RowCount());
}

}  // namespace
