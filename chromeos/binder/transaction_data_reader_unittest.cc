// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "chromeos/binder/transaction_data_reader.h"
#include "chromeos/binder/writable_transaction_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace binder {

TEST(BinderTransactionDataReaderTest, ReadData) {
  std::vector<char> input(3);
  for (size_t i = 0; i < input.size(); ++i) {
    input[i] = i;
  }
  WritableTransactionData data;
  data.WriteData(input.data(), input.size());
  EXPECT_EQ(4u, data.GetDataSize());  // Padded for 4-byte alignment.

  TransactionDataReader reader(data);
  EXPECT_TRUE(reader.HasMoreData());

  std::vector<char> result(input.size());
  EXPECT_TRUE(reader.ReadData(result.data(), result.size()));
  EXPECT_EQ(input, result);
  // Although we read only 3 bytes, we've already consumed 4 bytes because of
  // padding.
  EXPECT_FALSE(reader.HasMoreData());
}

TEST(BinderTransactionDataReaderTest, ReadScalarValues) {
  const int32 kInt32Value = -1;
  const uint32 kUint32Value = 2;
  const int64 kInt64Value = -3;
  const uint64 kUint64Value = 4;
  const float kFloatValue = 5.55;
  const double kDoubleValue = 6.66;

  WritableTransactionData data;
  data.WriteInt32(kInt32Value);
  data.WriteUint32(kUint32Value);
  data.WriteInt64(kInt64Value);
  data.WriteUint64(kUint64Value);
  data.WriteFloat(kFloatValue);
  data.WriteDouble(kDoubleValue);

  TransactionDataReader reader(data);
  EXPECT_TRUE(reader.HasMoreData());
  {
    int32 result = 0;
    EXPECT_TRUE(reader.ReadInt32(&result));
    EXPECT_EQ(kInt32Value, result);
  }
  {
    uint32 result = 0;
    EXPECT_TRUE(reader.ReadUint32(&result));
    EXPECT_EQ(kUint32Value, result);
  }
  {
    int64 result = 0;
    EXPECT_TRUE(reader.ReadInt64(&result));
    EXPECT_EQ(kInt64Value, result);
  }
  {
    uint64 result = 0;
    EXPECT_TRUE(reader.ReadUint64(&result));
    EXPECT_EQ(kUint64Value, result);
  }
  {
    float result = 0;
    EXPECT_TRUE(reader.ReadFloat(&result));
    EXPECT_EQ(kFloatValue, result);
  }
  {
    double result = 0;
    EXPECT_TRUE(reader.ReadDouble(&result));
    EXPECT_EQ(kDoubleValue, result);
  }
  EXPECT_FALSE(reader.HasMoreData());
}

}  // namespace binder
