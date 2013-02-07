// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"

#include <map>
#include <string>
#include "base/message_loop.h"
#include "chromeos/dbus/ibus/ibus_constants.h"
#include "dbus/message.h"
#include "dbus/mock_bus.h"
#include "dbus/mock_exported_object.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::Invoke;
using ::testing::Return;
using ::testing::StrEq;
using ::testing::_;

namespace chromeos {

class SynchronousCreateEngineHandler {
 public:
  explicit SynchronousCreateEngineHandler(const dbus::ObjectPath& path)
      : path_(path) {}

  void Run(const IBusEngineFactoryService::CreateEngineResponseSender& sender) {
    sender.Run(path_);
  }

 private:
  dbus::ObjectPath path_;

  DISALLOW_COPY_AND_ASSIGN(SynchronousCreateEngineHandler);
};

class AsynchronousCreateEngineHandler {
 public:
  AsynchronousCreateEngineHandler(const dbus::ObjectPath& path,
                                  MessageLoop* message_loop)
      : path_(path),
        message_loop_(message_loop) {}

  void Run(const IBusEngineFactoryService::CreateEngineResponseSender& sender) {
    message_loop_->PostTask(FROM_HERE, base::Bind(sender, path_));
  }

 private:
  dbus::ObjectPath path_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(AsynchronousCreateEngineHandler);
};

class MockCreateEngineResponseSender {
 public:
  explicit MockCreateEngineResponseSender(const dbus::ObjectPath& expected_path)
      : expected_path_(expected_path) {}
  // GMock doesn't support mocking methods which take scoped_ptr<>.
  MOCK_METHOD1(MockRun, void(dbus::Response*));
  void Run(scoped_ptr<dbus::Response> response) {
    MockRun(response.get());
  }

  // Checks the given |response| meets expectation for the CreateEngine method.
  void CheckCreateEngineResponsePtr(dbus::Response* response) {
    dbus::MessageReader reader(response);
    dbus::ObjectPath actual_path;
    ASSERT_TRUE(reader.PopObjectPath(&actual_path));
    EXPECT_EQ(expected_path_, actual_path);
  }
  void CheckCreateEngineResponse(scoped_ptr<dbus::Response> response) {
    CheckCreateEngineResponsePtr(response.get());
  }

 private:
  dbus::ObjectPath expected_path_;
};

class IBusEngineFactoryServiceTest : public testing::Test {
 public:
  IBusEngineFactoryServiceTest() {}

  virtual void SetUp() OVERRIDE {
    // Create a mock bus.
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    mock_bus_ = new dbus::MockBus(options);

    // Create a mock exported object.
    mock_exported_object_ = new dbus::MockExportedObject(
        mock_bus_.get(),
        dbus::ObjectPath(ibus::engine_factory::kServicePath));

    EXPECT_CALL(*mock_bus_,
                GetExportedObject(dbus::ObjectPath(
                    ibus::engine_factory::kServicePath)))
        .WillOnce(Return(mock_exported_object_.get()));

    EXPECT_CALL(*mock_bus_, AssertOnOriginThread())
        .WillRepeatedly(Return());

    EXPECT_CALL(*mock_exported_object_, ExportMethod(
        ibus::engine_factory::kServiceInterface,
        ibus::engine_factory::kCreateEngineMethod,
        _,
        _))
        .WillRepeatedly(
            Invoke(this, &IBusEngineFactoryServiceTest::OnMethodExported));

    service_.reset(IBusEngineFactoryService::Create(
        mock_bus_,
        REAL_DBUS_CLIENT_IMPLEMENTATION));
  }

 protected:
  // The service to be tested.
  scoped_ptr<IBusEngineFactoryService> service_;
  // The mock bus.
  scoped_refptr<dbus::MockBus> mock_bus_;
  // The mock exported object.
  scoped_refptr<dbus::MockExportedObject> mock_exported_object_;
  // The map from method name to method call handler.
  std::map<std::string, dbus::ExportedObject::MethodCallCallback>
      method_exported_map_;
  // A message loop to emulate asynchronous behavior.
  MessageLoop message_loop_;

 private:
  // Used to implement the method call exportation.
  void OnMethodExported(
      const std::string& interface_name,
      const std::string& method_name,
      const dbus::ExportedObject::MethodCallCallback& method_callback,
      const dbus::ExportedObject::OnExportedCallback& on_exported_callback) {
    method_exported_map_[method_name] = method_callback;
    const bool success = true;
    message_loop_.PostTask(FROM_HERE, base::Bind(on_exported_callback,
                                                 interface_name,
                                                 method_name,
                                                 success));
  }
};

TEST_F(IBusEngineFactoryServiceTest, SyncCreateEngineTest) {
  // Set expectations.
  const char kSampleEngine[] = "Sample Engine";
  const dbus::ObjectPath kObjectPath("/org/freedesktop/IBus/Engine/10");
  MockCreateEngineResponseSender response_sender(kObjectPath);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(
          Invoke(&response_sender,
                 &MockCreateEngineResponseSender::
                 CheckCreateEngineResponsePtr));

  SynchronousCreateEngineHandler handler(kObjectPath);
  // Set handler expectations.
  service_->SetCreateEngineHandler(
      kSampleEngine,
      base::Bind(&SynchronousCreateEngineHandler::Run,
                 base::Unretained(&handler)));
  message_loop_.RunUntilIdle();

  // Invoke method call.
  dbus::MethodCall method_call(
      ibus::engine_factory::kServiceInterface,
      ibus::engine_factory::kCreateEngineMethod);
  method_call.SetSerial(10);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kSampleEngine);
  ASSERT_FALSE(
      method_exported_map_[ibus::engine_factory::kCreateEngineMethod]
          .is_null());
  method_exported_map_[ibus::engine_factory::kCreateEngineMethod].Run(
      &method_call,
      base::Bind(&MockCreateEngineResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Unset the handler so expect not calling handler.
  service_->UnsetCreateEngineHandler(kSampleEngine);
  method_exported_map_[ibus::engine_factory::kCreateEngineMethod].Run(
      &method_call,
      base::Bind(&MockCreateEngineResponseSender::CheckCreateEngineResponse,
                 base::Unretained(&response_sender)));
  message_loop_.RunUntilIdle();
}

TEST_F(IBusEngineFactoryServiceTest, AsyncCreateEngineTest) {
  // Set expectations.
  const char kSampleEngine[] = "Sample Engine";
  const dbus::ObjectPath kObjectPath("/org/freedesktop/IBus/Engine/10");
  MockCreateEngineResponseSender response_sender(kObjectPath);
  EXPECT_CALL(response_sender, MockRun(_))
      .WillOnce(
          Invoke(&response_sender,
                 &MockCreateEngineResponseSender::
                 CheckCreateEngineResponsePtr));

  AsynchronousCreateEngineHandler handler(kObjectPath, &message_loop_);
  // Set handler expectations.
  service_->SetCreateEngineHandler(
      kSampleEngine,
      base::Bind(&AsynchronousCreateEngineHandler::Run,
                 base::Unretained(&handler)));
  message_loop_.RunUntilIdle();

  // Invoke method call.
  dbus::MethodCall method_call(
      ibus::engine_factory::kServiceInterface,
      ibus::engine_factory::kCreateEngineMethod);
  method_call.SetSerial(10);
  dbus::MessageWriter writer(&method_call);
  writer.AppendString(kSampleEngine);
  ASSERT_FALSE(
      method_exported_map_[ibus::engine_factory::kCreateEngineMethod]
          .is_null());
  method_exported_map_[ibus::engine_factory::kCreateEngineMethod].Run(
      &method_call,
      base::Bind(&MockCreateEngineResponseSender::Run,
                 base::Unretained(&response_sender)));

  // Unset the handler so expect not calling handler.
  service_->UnsetCreateEngineHandler(kSampleEngine);
  method_exported_map_[ibus::engine_factory::kCreateEngineMethod].Run(
      &method_call,
      base::Bind(&MockCreateEngineResponseSender::CheckCreateEngineResponse,
                 base::Unretained(&response_sender)));
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
