// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sessions/compress_data_helper.h"

#include "base/pickle.h"
#include "testing/gtest/include/gtest/gtest.h"

typedef testing::Test CompressDataHelperTest;

TEST_F(CompressDataHelperTest, CompressAndDecompressData) {
  std::string data = "test data for compression and decompression";
  // Add NULL bytes.
  data[3] = data[8] = 0;

  Pickle pickle;
  int bytes_written = 0;
  int max_bytes = 100;
  CompressDataHelper::CompressAndWriteStringToPickle(
      data, max_bytes, &pickle, &bytes_written);

  EXPECT_GT(bytes_written, 0);
  EXPECT_LT(bytes_written, max_bytes + 1);

  PickleIterator it(pickle);
  std::string data_out;

  ASSERT_TRUE(CompressDataHelper::ReadAndDecompressStringFromPickle(
      pickle, &it, &data_out));

  ASSERT_EQ(data.size(), data_out.size());
  for (size_t i = 0; i < data_out.size(); ++i)
    EXPECT_EQ(data_out[i], data[i]);
}

TEST_F(CompressDataHelperTest, CompressAndDecompressEmptyData) {
  std::string empty;

  Pickle pickle;
  int bytes_written = 0;
  int max_bytes = 100;
  CompressDataHelper::CompressAndWriteStringToPickle(
      empty, max_bytes, &pickle, &bytes_written);

  EXPECT_EQ(0, bytes_written);

  PickleIterator it(pickle);
  std::string data_out;
  ASSERT_TRUE(CompressDataHelper::ReadAndDecompressStringFromPickle(
      pickle, &it, &data_out));

  EXPECT_EQ(0U, data_out.size());
}

TEST_F(CompressDataHelperTest, TooMuchData) {
  std::string data = "Some data that clearly cannot be compressed to 2 bytes";

  Pickle pickle;
  int bytes_written = 0;
  int max_bytes = 2;
  CompressDataHelper::CompressAndWriteStringToPickle(
      data, max_bytes, &pickle, &bytes_written);

  EXPECT_EQ(0, bytes_written);

  // When the data is read, we get an empty string back.
  PickleIterator it(pickle);
  std::string data_out;
  ASSERT_TRUE(CompressDataHelper::ReadAndDecompressStringFromPickle(
      pickle, &it, &data_out));

  EXPECT_EQ(0U, data_out.size());
}
