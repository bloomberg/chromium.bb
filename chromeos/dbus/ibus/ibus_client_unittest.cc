// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_client.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Invoke;
using testing::Return;
using testing::_;

namespace chromeos {

namespace {
const char kClientName[] = "chrome";
}  // namespace

class MockCreateInputContextCallback {
 public:
  MOCK_METHOD1(Run, void(const dbus::ObjectPath& object_path));
};

class IBusClientTest : public testing::Test {
 public:
  IBusClientTest() : response_(NULL) {}

  // Handles CreateInputContext method call.
  void OnCreateInputContext(
      dbus::MethodCall* method_call,
      int timeout_ms,
      const dbus::ObjectProxy::ResponseCallback& callback) {
    dbus::MessageReader reader(method_call);
    std::string client_name;
    EXPECT_TRUE(reader.PopString(&client_name));
    EXPECT_EQ(kClientName, client_name);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.PostTask(FROM_HERE, base::Bind(callback, response_));
  }

 protected:
  virtual void SetUp() OVERRIDE {
    dbus::Bus::Options options;
    mock_bus_ = new dbus::MockBus(options);
    mock_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(),
                                            kIBusServiceName,
                                            dbus::ObjectPath(kIBusServicePath));
    EXPECT_CALL(*mock_bus_, GetObjectProxy(kIBusServiceName,
                                           dbus::ObjectPath(kIBusServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    EXPECT_CALL(*mock_bus_, ShutdownAndBlock());
    client_.reset(IBusClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                     mock_bus_));
  }

  virtual void TearDown() OVERRIDE {
    mock_bus_->ShutdownAndBlock();
  }

  // The IBus client to be tested.
  scoped_ptr<IBusClient> client_;

  // A message loop to emulate asynchronous behavior.
  MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;

  // Response returned by mock methods.
  dbus::Response* response_;
};

TEST_F(IBusClientTest, CreateInputContextTest) {
  // Set expectations
  const dbus::ObjectPath kInputContextObjectPath =
      dbus::ObjectPath("some.object.path");
  EXPECT_CALL(*mock_proxy_, CallMethod(_, _, _))
      .WillOnce(Invoke(this, &IBusClientTest::OnCreateInputContext));
  MockCreateInputContextCallback callback;
  EXPECT_CALL(callback, Run(kInputContextObjectPath));

  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(kInputContextObjectPath);
  response_ = response.get();

  // Call CreateInputContext
  client_->CreateInputContext(kClientName,
                              base::Bind(&MockCreateInputContextCallback::Run,
                                         base::Unretained(&callback)));

  // Run the message loop.
  message_loop_.RunAllPending();
}

}  // namespace chromeos
