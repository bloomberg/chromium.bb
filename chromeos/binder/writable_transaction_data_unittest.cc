// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "chromeos/binder/buffer_reader.h"
#include "chromeos/binder/writable_transaction_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace binder {

TEST(BinderWritableTransactionDataTest, WriteData) {
  char array[3];
  for (size_t i = 0; i < sizeof(array); ++i) {
    array[i] = i;
  }

  WritableTransactionData data;
  EXPECT_EQ(0u, data.GetDataSize());
  data.WriteData(array, sizeof(array));
  EXPECT_EQ(4u, data.GetDataSize());  // Padded for 4-byte alignment.
  for (size_t i = 0; i < sizeof(array); ++i) {
    SCOPED_TRACE(i);
    EXPECT_EQ(array[i], reinterpret_cast<const char*>(data.GetData())[i]);
  }
}

TEST(BinderWritableTransactionDataTest, WriteInt32) {
  const int32_t kValue = -1234;
  WritableTransactionData data;
  data.WriteInt32(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  int32_t result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderWritableTransactionDataTest, WriteUint32) {
  const uint32_t kValue = 1234;
  WritableTransactionData data;
  data.WriteUint32(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  uint32_t result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderWritableTransactionDataTest, WriteInt64) {
  const int64_t kValue = -1234;
  WritableTransactionData data;
  data.WriteInt64(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  int64_t result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderWritableTransactionDataTest, WriteUint64) {
  const uint64_t kValue = 1234;
  WritableTransactionData data;
  data.WriteUint64(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  uint64_t result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderWritableTransactionDataTest, WriteFloat) {
  const float kValue = 1234.5678;
  WritableTransactionData data;
  data.WriteFloat(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  float result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderWritableTransactionDataTest, WriteDouble) {
  const double kValue = 1234.5678;
  WritableTransactionData data;
  data.WriteDouble(kValue);
  EXPECT_EQ(sizeof(kValue), data.GetDataSize());
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  double result = 0;
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(kValue, result);
  EXPECT_FALSE(reader.HasMoreData());
}

}  // namespace binder
