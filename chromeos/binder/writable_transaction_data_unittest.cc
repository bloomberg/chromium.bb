// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <linux/android/binder.h>

#include "base/message_loop/message_loop.h"
#include "chromeos/binder/buffer_reader.h"
#include "chromeos/binder/command_broker.h"
#include "chromeos/binder/driver.h"
#include "chromeos/binder/local_object.h"
#include "chromeos/binder/remote_object.h"
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

TEST(BinderWritableTransactionDataTest, WriteObject) {
  base::MessageLoopForIO message_loop;

  Driver driver;
  ASSERT_TRUE(driver.Initialize());
  CommandBroker command_broker(&driver);

  scoped_refptr<LocalObject> local(
      new LocalObject(scoped_ptr<LocalObject::TransactionHandler>()));

  const int32_t kDummyHandle = 42;
  scoped_refptr<RemoteObject> remote(
      new RemoteObject(&command_broker, kDummyHandle));

  // Write a local object & a remote object.
  WritableTransactionData data;
  data.WriteObject(local);
  data.WriteObject(remote);

  // Check object offsets.
  ASSERT_EQ(2u, data.GetNumObjectOffsets());
  EXPECT_EQ(0u, data.GetObjectOffsets()[0]);
  EXPECT_EQ(sizeof(flat_binder_object), data.GetObjectOffsets()[1]);

  // Check the written local object.
  BufferReader reader(reinterpret_cast<const char*>(data.GetData()),
                      data.GetDataSize());
  flat_binder_object result = {};
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(BINDER_TYPE_BINDER, result.type);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(local.get()), result.cookie);
  EXPECT_EQ(reinterpret_cast<uintptr_t>(local.get()), result.binder);

  // Check the written remote object.
  EXPECT_TRUE(reader.Read(&result, sizeof(result)));
  EXPECT_EQ(BINDER_TYPE_HANDLE, result.type);
  EXPECT_EQ(kDummyHandle, result.handle);
}

}  // namespace binder
