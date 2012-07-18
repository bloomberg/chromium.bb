// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_engine_factory_service.h"

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

class MockCreateEngineHandler {
 public:
  MOCK_METHOD1(Run, dbus::ObjectPath(const std::string& engine_name));
};

class MockCreateEngineResponseSender {
 public:
  MockCreateEngineResponseSender(const dbus::ObjectPath expected_path)
      : expected_path_(expected_path) {}
  MOCK_METHOD1(Run, void(dbus::Response*));

  // Checks the given |response| meets expectation for the CreateEngine method.
  void CheckCreateEngineResponse(dbus::Response* response) {
    dbus::MessageReader reader(response);
    dbus::ObjectPath actual_path;
    ASSERT_TRUE(reader.PopObjectPath(&actual_path));
    EXPECT_EQ(expected_path_, actual_path);
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

TEST_F(IBusEngineFactoryServiceTest, CreateEngineTest) {
  // Set expectations.
  const char kSampleEngine[] = "Sample Engine";
  const dbus::ObjectPath kObjectPath("/org/freedesktop/IBus/Engine/10");
  MockCreateEngineResponseSender response_sender(kObjectPath);
  EXPECT_CALL(response_sender, Run(_))
      .WillOnce(
          Invoke(&response_sender,
                 &MockCreateEngineResponseSender::CheckCreateEngineResponse));

  // Set handler expectations.
  MockCreateEngineHandler handler;
  EXPECT_CALL(handler, Run(StrEq(kSampleEngine)))
      .WillOnce(Return(kObjectPath));
  service_->SetCreateEngineHandler(base::Bind(&MockCreateEngineHandler::Run,
                                             base::Unretained(&handler)));
  message_loop_.RunAllPending();

  // Invoke method call.
  dbus::MethodCall method_call(
      ibus::engine_factory::kServiceInterface,
      ibus::engine_factory::kCreateEngineMethod);
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
  service_->UnsetCreateEngineHandler();
  method_exported_map_[ibus::engine_factory::kCreateEngineMethod].Run(
      &method_call,
      base::Bind(&MockCreateEngineResponseSender::CheckCreateEngineResponse,
                 base::Unretained(&response_sender)));

  message_loop_.RunAllPending();
}

}  // namespace chromeos
