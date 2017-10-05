// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/modem_messaging_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_object_proxy.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using ::testing::_;
using ::testing::Invoke;
using ::testing::Return;

namespace chromeos {

namespace {

// A mock SmsReceivedHandler.
class MockSmsReceivedHandler {
 public:
  MOCK_METHOD2(Run, void(const dbus::ObjectPath &sms, bool complete));
};

// A mock DeleteCallback.
class MockDeleteCallback {
 public:
  MOCK_METHOD0(Run, void());
};

// A mock ListCallback.
class MockListCallback {
 public:
  MOCK_METHOD1(Run, void(const std::vector<dbus::ObjectPath>& result));
};

// D-Bus service name used by test.
const char kServiceName[] = "service.name";
// D-Bus object path used by test.
const char kObjectPath[] = "/object/path";

}  // namespace

class ModemMessagingClientTest : public testing::Test {
 public:
  ModemMessagingClientTest() : response_(NULL),
                               expected_result_(NULL) {}

  void SetUp() override {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock proxy.
    mock_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(),
                                            kServiceName,
                                            dbus::ObjectPath(kObjectPath));

    // Set an expectation so mock_proxy's ConnectToSignal() will use
    // OnConnectToSignal() to run the callback.
    EXPECT_CALL(
        *mock_proxy_.get(),
        DoConnectToSignal(modemmanager::kModemManager1MessagingInterface,
                          modemmanager::kSMSAddedSignal, _, _))
        .WillRepeatedly(
            Invoke(this, &ModemMessagingClientTest::OnConnectToSignal));

    // Set an expectation so mock_bus's GetObjectProxy() for the given
    // service name and the object path will return mock_proxy_.
    EXPECT_CALL(*mock_bus_.get(),
                GetObjectProxy(kServiceName, dbus::ObjectPath(kObjectPath)))
        .WillOnce(Return(mock_proxy_.get()));

    // ShutdownAndBlock() will be called in TearDown().
    EXPECT_CALL(*mock_bus_.get(), ShutdownAndBlock()).WillOnce(Return());

    // Create a client with the mock bus.
    client_.reset(ModemMessagingClient::Create());
    client_->Init(mock_bus_.get());
  }

  void TearDown() override { mock_bus_->ShutdownAndBlock(); }

  // Handles Delete method call.
  void OnDelete(dbus::MethodCall* method_call,
                int timeout_ms,
                dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(modemmanager::kModemManager1MessagingInterface,
              method_call->GetInterface());
    EXPECT_EQ(modemmanager::kSMSDeleteFunction, method_call->GetMember());
    dbus::ObjectPath sms_path;
    dbus::MessageReader reader(method_call);
    EXPECT_TRUE(reader.PopObjectPath(&sms_path));
    EXPECT_EQ(expected_sms_path_, sms_path);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // Handles List method call.
  void OnList(dbus::MethodCall* method_call,
              int timeout_ms,
              dbus::ObjectProxy::ResponseCallback* callback) {
    EXPECT_EQ(modemmanager::kModemManager1MessagingInterface,
              method_call->GetInterface());
    EXPECT_EQ(modemmanager::kSMSListFunction, method_call->GetMember());
    dbus::MessageReader reader(method_call);
    EXPECT_FALSE(reader.HasMoreData());

    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*callback), response_));
  }

  // Checks the results of List.
  void CheckResult(const std::vector<dbus::ObjectPath>& result) {
    EXPECT_EQ(result, *expected_result_);
  }

 protected:
  // The client to be tested.
  std::unique_ptr<ModemMessagingClient> client_;
  // A message loop to emulate asynchronous behavior.
  base::MessageLoop message_loop_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock object proxy.
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
  // The SmsReceived signal handler given by the tested client.
  dbus::ObjectProxy::SignalCallback sms_received_callback_;
  // Expected argument for Delete method.
  dbus::ObjectPath expected_sms_path_;
  // Response returned by mock methods.
  dbus::Response* response_;
  // Expected result of List method.
  std::vector<dbus::ObjectPath>* expected_result_;

 private:
  // Used to implement the mock proxy.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      dbus::ObjectProxy::OnConnectedCallback* on_connected_callback) {
    sms_received_callback_ = signal_callback;
    const bool success = true;
    message_loop_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(std::move(*on_connected_callback),
                                  interface_name, signal_name, success));
  }
};

TEST_F(ModemMessagingClientTest, SmsReceived) {
  // Set expectations.
  const dbus::ObjectPath kSmsPath("/SMS/0");
  const bool kComplete = true;
  MockSmsReceivedHandler handler;
  EXPECT_CALL(handler, Run(kSmsPath, kComplete)).Times(1);
  // Set handler.
  client_->SetSmsReceivedHandler(kServiceName, dbus::ObjectPath(kObjectPath),
                                 base::Bind(&MockSmsReceivedHandler::Run,
                                            base::Unretained(&handler)));

  // Run the message loop to run the signal connection result callback.
  base::RunLoop().RunUntilIdle();

  // Send signal.
  dbus::Signal signal(modemmanager::kModemManager1MessagingInterface,
                      modemmanager::kSMSAddedSignal);
  dbus::MessageWriter writer(&signal);
  writer.AppendObjectPath(kSmsPath);
  writer.AppendBool(kComplete);
  ASSERT_FALSE(sms_received_callback_.is_null());
  sms_received_callback_.Run(&signal);
  // Reset handler.
  client_->ResetSmsReceivedHandler(kServiceName, dbus::ObjectPath(kObjectPath));
  // Send signal again.
  sms_received_callback_.Run(&signal);
}

TEST_F(ModemMessagingClientTest, Delete) {
  // Set expectations.
  const dbus::ObjectPath kSmsPath("/SMS/0");
  expected_sms_path_ = kSmsPath;
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &ModemMessagingClientTest::OnDelete));
  MockDeleteCallback callback;
  EXPECT_CALL(callback, Run()).Times(1);
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  response_ = response.get();
  // Call Delete.
  client_->Delete(kServiceName, dbus::ObjectPath(kObjectPath), kSmsPath,
                  base::Bind(&MockDeleteCallback::Run,
                             base::Unretained(&callback)));

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ModemMessagingClientTest, List) {
  // Set expectations.
  EXPECT_CALL(*mock_proxy_.get(), DoCallMethod(_, _, _))
      .WillOnce(Invoke(this, &ModemMessagingClientTest::OnList));
  MockListCallback callback;
  EXPECT_CALL(callback, Run(_))
      .WillOnce(Invoke(this, &ModemMessagingClientTest::CheckResult));
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::ObjectPath path1("/SMS/1");
  dbus::ObjectPath path2("/SMS/2");
  std::vector<dbus::ObjectPath> expected_result;
  expected_result.push_back(path1);
  expected_result.push_back(path2);

  dbus::MessageWriter writer(response.get());
  writer.AppendArrayOfObjectPaths(expected_result);
  response_ = response.get();

  // Save expected result.
  expected_result_ = &expected_result;
  // Call List.
  client_->List(kServiceName, dbus::ObjectPath(kObjectPath),
                base::Bind(&MockListCallback::Run,
                           base::Unretained(&callback)));

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
