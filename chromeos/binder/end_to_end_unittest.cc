// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_util.h"
#include "base/message_loop/message_loop.h"
#include "chromeos/binder/command_broker.h"
#include "chromeos/binder/driver.h"
#include "chromeos/binder/object.h"
#include "chromeos/binder/service_manager_proxy.h"
#include "chromeos/binder/test_service.h"
#include "chromeos/binder/transaction_data.h"
#include "chromeos/binder/transaction_data_reader.h"
#include "chromeos/binder/writable_transaction_data.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace binder {

class BinderEndToEndTest : public ::testing::Test {
 public:
  BinderEndToEndTest() : command_broker_(&driver_) {}

  void SetUp() override {
    ASSERT_TRUE(driver_.Initialize());

    // Start the test service and get a remote object from it.
    ASSERT_TRUE(test_service_.StartAndWait());
    remote_object_ = ServiceManagerProxy::CheckService(
        &command_broker_, test_service_.service_name());
    ASSERT_EQ(Object::TYPE_REMOTE, remote_object_->GetType());
    ASSERT_TRUE(remote_object_);
  }

 protected:
  base::MessageLoopForIO message_loop_;
  Driver driver_;
  CommandBroker command_broker_;
  TestService test_service_;
  scoped_refptr<Object> remote_object_;
};

TEST_F(BinderEndToEndTest, IncrementInt) {
  const int32_t kInput = 42;
  WritableTransactionData data;
  data.SetCode(TestService::INCREMENT_INT_TRANSACTION);
  data.WriteInt32(kInput);
  scoped_ptr<TransactionData> reply;
  ASSERT_TRUE(remote_object_->Transact(&command_broker_, data, &reply));
  ASSERT_TRUE(reply);

  TransactionDataReader reader(*reply);
  int32_t result = 0;
  EXPECT_TRUE(reader.ReadInt32(&result));
  EXPECT_EQ(kInput + 1, result);
}

TEST_F(BinderEndToEndTest, GetFD) {
  WritableTransactionData data;
  data.SetCode(TestService::GET_FD_TRANSACTION);
  scoped_ptr<TransactionData> reply;
  ASSERT_TRUE(remote_object_->Transact(&command_broker_, data, &reply));
  ASSERT_TRUE(reply);

  TransactionDataReader reader(*reply);
  int fd = -1;
  EXPECT_TRUE(reader.ReadFileDescriptor(&fd));

  const std::string kExpected = TestService::GetFileContents();
  std::vector<char> buf(kExpected.size());
  EXPECT_TRUE(base::ReadFromFD(fd, buf.data(), buf.size()));
  EXPECT_EQ(kExpected, std::string(buf.data(), buf.size()));
}

}  // namespace binder
