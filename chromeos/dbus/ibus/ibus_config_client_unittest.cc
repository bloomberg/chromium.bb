// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_config_client.h"

#include <vector>

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

namespace {
const char kSection[] = "IBus/Section";
const char kKey[] = "key";

enum HandlerBehavior {
  HANDLER_SUCCESS,
  HANDLER_FAIL,
};
}  // namespace

namespace chromeos {

class MockErrorCallback {
 public:
  MOCK_METHOD0(Run, void());
};

class MockOnIBusConfigReadyCallback {
 public:
  MOCK_METHOD0(Run, void());
};

// The base class of each type specified SetValue function handler. This class
// checks "section" and "key" field in SetValue message and callback success or
// error callback based on given |success| flag.
class SetValueVerifierBase {
 public:
  SetValueVerifierBase(const std::string& expected_section,
                       const std::string& expected_key,
                       HandlerBehavior behavior)
      : expected_section_(expected_section),
        expected_key_(expected_key),
        behavior_(behavior) {}

  virtual ~SetValueVerifierBase() {}

  // Handles SetValue method call. This function checks "section" and "key"
  // field. For the "value" field, subclass checks it with over riding
  // VeirfyVariant member function.
  void Run(dbus::MethodCall* method_call,
           int timeout_ms,
           const dbus::ObjectProxy::ResponseCallback& callback,
           const dbus::ObjectProxy::ErrorCallback& error_callback) {
    dbus::MessageReader reader(method_call);
    std::string section;
    std::string key;

    EXPECT_TRUE(reader.PopString(&section));
    EXPECT_EQ(expected_section_, section);
    EXPECT_TRUE(reader.PopString(&key));
    EXPECT_EQ(expected_key_, key);
    dbus::MessageReader variant_reader(NULL);
    EXPECT_TRUE(reader.PopVariant(&variant_reader));

    VerifyVariant(&variant_reader);

    EXPECT_FALSE(reader.HasMoreData());
    if (behavior_ == HANDLER_SUCCESS) {
      scoped_ptr<dbus::Response> response(
          dbus::Response::FromMethodCall(method_call));
      callback.Run(response.get());
    } else {
      scoped_ptr<dbus::ErrorResponse> error_response(
          dbus::ErrorResponse::FromMethodCall(method_call, "Error",
                                              "Error Message"));
      error_callback.Run(error_response.get());
    }
  }

  // Verifies "value" field in SetValue method call.
  virtual void VerifyVariant(dbus::MessageReader* reader) = 0;

 private:
  const std::string expected_section_;
  const std::string expected_key_;
  const HandlerBehavior behavior_;

  DISALLOW_COPY_AND_ASSIGN(SetValueVerifierBase);
};

// The class to verify SetStringValue method call arguments.
class SetStringValueHandler : public SetValueVerifierBase {
 public:
  SetStringValueHandler(const std::string& expected_key,
                        const std::string& expected_section,
                        const std::string& expected_value,
                        HandlerBehavior behavior)
      : SetValueVerifierBase(expected_section, expected_key, behavior),
        expected_value_(expected_value) {}


  // SetValueVerifierBase override.
  virtual void VerifyVariant(dbus::MessageReader* reader) OVERRIDE {
    std::string value;
    EXPECT_TRUE(reader->PopString(&value));
    EXPECT_EQ(expected_value_, value);
  }

 private:
  const std::string expected_value_;

  DISALLOW_COPY_AND_ASSIGN(SetStringValueHandler);
};

// The class to verify SetIntValue method call arguments.
class SetIntValueHandler : public SetValueVerifierBase {
 public:
  SetIntValueHandler(const std::string& expected_key,
                     const std::string& expected_section,
                     int expected_value,
                     HandlerBehavior behavior)
      : SetValueVerifierBase(expected_section, expected_key, behavior),
        expected_value_(expected_value) {}

  // SetValueVerifierBase override.
  virtual void VerifyVariant(dbus::MessageReader* reader) OVERRIDE {
    int value = -1;
    EXPECT_TRUE(reader->PopInt32(&value));
    EXPECT_EQ(expected_value_, value);
  }

 private:
  const int expected_value_;

  DISALLOW_COPY_AND_ASSIGN(SetIntValueHandler);
};

// The class to verify SetBoolValue method call arguments.
class SetBoolValueHandler : public SetValueVerifierBase {
 public:
  SetBoolValueHandler(const std::string& expected_key,
                      const std::string& expected_section,
                      bool expected_value,
                      HandlerBehavior behavior)
      : SetValueVerifierBase(expected_section, expected_key, behavior),
        expected_value_(expected_value) {}

  // SetValueVerifierBase override.
  virtual void VerifyVariant(dbus::MessageReader* reader) OVERRIDE {
    bool value = false;
    EXPECT_TRUE(reader->PopBool(&value));
    EXPECT_EQ(expected_value_, value);
  }

 private:
  const bool expected_value_;

  DISALLOW_COPY_AND_ASSIGN(SetBoolValueHandler);
};

// The class to verify SetStringListValue method call arguments.
class SetStringListValueHandler : public SetValueVerifierBase {
 public:
  SetStringListValueHandler(const std::string& expected_key,
                            const std::string& expected_section,
                            const std::vector<std::string>& expected_value,
                            HandlerBehavior behavior)
      : SetValueVerifierBase(expected_section, expected_key, behavior),
        expected_value_(expected_value) {}

  // SetValueVerifierBase override
  virtual void VerifyVariant(dbus::MessageReader* reader) OVERRIDE {
    dbus::MessageReader array_reader(NULL);
    ASSERT_TRUE(reader->PopArray(&array_reader));
    for (size_t i = 0; i < expected_value_.size(); ++i) {
      std::string value;
      ASSERT_TRUE(array_reader.HasMoreData());
      EXPECT_TRUE(array_reader.PopString(&value));
      EXPECT_EQ(expected_value_[i], value);
    }
    EXPECT_FALSE(array_reader.HasMoreData());
  }

 private:
  const std::vector<std::string>& expected_value_;

  DISALLOW_COPY_AND_ASSIGN(SetStringListValueHandler);
};

// The class verifies GetNameOwner method call and emits response callback
// asynchronouslly.
class MockGetNameOwnerMethodCallHandler {
 public:
  MockGetNameOwnerMethodCallHandler() {}

  // Handles CallMethod function.
  void Run(dbus::MethodCall* method_call,
           int timeout_ms,
           const dbus::ObjectProxy::ResponseCallback& callback,
           const dbus::ObjectProxy::ErrorCallback& error_callback) {
    dbus::MessageReader reader(method_call);
    std::string target_name;
    EXPECT_TRUE(reader.PopString(&target_name));
    EXPECT_EQ(ibus::config::kServiceName, target_name);
    EXPECT_FALSE(reader.HasMoreData());

    callback_ = callback;
  }

  // Invokes reply with given |callback_| in Run method.
  void EmitReplyCallback(const std::string& owner) {
    scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
    dbus::MessageWriter writer(response.get());
    writer.AppendString(owner);
    callback_.Run(response.get());
  }

 private:
  dbus::ObjectProxy::ResponseCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(MockGetNameOwnerMethodCallHandler);
};


// The class emulate GetNameOwner signal.
class NameOwnerChangedHandler {
 public:
  NameOwnerChangedHandler() {}

  // OnConnectToSignal mock function.
  void OnConnectToSignal(
      const std::string& interface_name,
      const std::string& signal_name,
      const dbus::ObjectProxy::SignalCallback& signal_callback,
      const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
    callback_ = signal_callback;
    on_connected_callback.Run(interface_name, signal_name, true);
  }

  // Invokes reply with given |callback_| in Run method.
  void InvokeSignal(const std::string& name,
                    const std::string& old_owner,
                    const std::string& new_owner) {
    dbus::Signal signal(ibus::kDBusInterface, ibus::kGetNameOwnerMethod);
    dbus::MessageWriter writer(&signal);
    writer.AppendString(name);
    writer.AppendString(old_owner);
    writer.AppendString(new_owner);
    callback_.Run(&signal);
  }

 private:
  dbus::ObjectProxy::SignalCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(NameOwnerChangedHandler);
};


class IBusConfigClientTest : public testing::Test {
 public:
  IBusConfigClientTest() {}

 protected:
  virtual void SetUp() OVERRIDE {
    dbus::Bus::Options options;
    mock_bus_ = new dbus::MockBus(options);
    mock_proxy_ = new dbus::MockObjectProxy(mock_bus_.get(),
                                            ibus::kServiceName,
                                            dbus::ObjectPath(
                                                ibus::config::kServicePath));
    EXPECT_CALL(*mock_bus_, ShutdownAndBlock());
    client_.reset(IBusConfigClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                           mock_bus_));

    // Surpress uninteresting mock function call warning.
    EXPECT_CALL(*mock_bus_.get(), AssertOnOriginThread())
        .WillRepeatedly(Return());
  }

  virtual void TearDown() OVERRIDE {
    mock_bus_->ShutdownAndBlock();
  }

  // Initialize |client_| by replying valid owner name synchronously.
  void InitializeSync() {
    EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::config::kServiceName,
                                           dbus::ObjectPath(
                                               ibus::config::kServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy
        = new dbus::MockObjectProxy(mock_bus_.get(),
                                    ibus::kDBusServiceName,
                                    dbus::ObjectPath(ibus::kDBusObjectPath));
    EXPECT_CALL(*mock_bus_,
                GetObjectProxy(ibus::kDBusServiceName,
                               dbus::ObjectPath(ibus::kDBusObjectPath)))
        .WillOnce(Return(mock_dbus_proxy.get()));

    MockGetNameOwnerMethodCallHandler mock_get_name_owner_method_call;
    EXPECT_CALL(*mock_dbus_proxy, CallMethodWithErrorCallback(_, _, _, _))
        .WillOnce(Invoke(&mock_get_name_owner_method_call,
                         &MockGetNameOwnerMethodCallHandler::Run));
    NameOwnerChangedHandler handler;
    EXPECT_CALL(*mock_dbus_proxy, ConnectToSignal(
        ibus::kDBusInterface,
        ibus::kNameOwnerChangedSignal,
        _,
        _)).WillOnce(Invoke(&handler,
                            &NameOwnerChangedHandler::OnConnectToSignal));
    client_->InitializeAsync(base::Bind(&base::DoNothing));
    mock_get_name_owner_method_call.EmitReplyCallback(":0.1");
  }

  // The IBus config client to be tested.
  scoped_ptr<IBusConfigClient> client_;

  // A message loop to emulate asynchronous behavior.
  MessageLoop message_loop_;
  scoped_refptr<dbus::MockBus> mock_bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_proxy_;
};

TEST_F(IBusConfigClientTest, SetStringValueTest) {
  // Set expectations
  InitializeSync();
  const char value[] = "value";
  SetStringValueHandler handler(kSection, kKey, value, HANDLER_SUCCESS);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);

  // Call SetStringValue.
  client_->SetStringValue(kKey, kSection, value,
                          base::Bind(&MockErrorCallback::Run,
                                     base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetStringValueTest_Fail) {
  // Set expectations
  InitializeSync();
  const char value[] = "value";
  SetStringValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run());

  // Call SetStringValue.
  client_->SetStringValue(kKey, kSection, value,
                          base::Bind(&MockErrorCallback::Run,
                                     base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetIntValueTest) {
  // Set expectations
  InitializeSync();
  const int value = 1234;
  SetIntValueHandler handler(kSection, kKey, value, HANDLER_SUCCESS);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);

  // Call SetIntValue.
  client_->SetIntValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetIntValueTest_Fail) {
  // Set expectations
  InitializeSync();
  const int value = 1234;
  SetIntValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run());

  // Call SetIntValue.
  client_->SetIntValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetBoolValueTest) {
  // Set expectations
  InitializeSync();
  const bool value = true;
  SetBoolValueHandler handler(kSection, kKey, value, HANDLER_SUCCESS);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);

  // Call SetBoolValue.
  client_->SetBoolValue(kKey, kSection, value,
                        base::Bind(&MockErrorCallback::Run,
                                   base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetBoolValueTest_Fail) {
  // Set expectations
  InitializeSync();
  const bool value = true;
  SetBoolValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run());

  // Call SetBoolValue.
  client_->SetBoolValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetStringListValueTest) {
  // Set expectations
  InitializeSync();
  std::vector<std::string> value;
  value.push_back("Sample value 1");
  value.push_back("Sample value 2");

  SetStringListValueHandler handler(kSection, kKey, value, HANDLER_SUCCESS);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);

  // Call SetStringListValue.
  client_->SetStringListValue(kKey, kSection, value,
                              base::Bind(&MockErrorCallback::Run,
                                         base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, SetStringListValueTest_Fail) {
  // Set expectations
  InitializeSync();
  std::vector<std::string> value;
  value.push_back("Sample value 1");
  value.push_back("Sample value 2");

  SetStringListValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run());

  // Call SetStringListValue.
  client_->SetStringListValue(kKey, kSection, value,
                              base::Bind(&MockErrorCallback::Run,
                                         base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, IBusConfigDaemon_NotAvailableTest) {
  MockGetNameOwnerMethodCallHandler mock_get_name_owner_method_call;

  EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::kServiceName,
                                         dbus::ObjectPath(
                                             ibus::config::kServicePath)))
      .Times(0);

  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy
      = new dbus::MockObjectProxy(mock_bus_.get(),
                                  ibus::kDBusServiceName,
                                  dbus::ObjectPath(ibus::kDBusObjectPath));
  EXPECT_CALL(*mock_bus_,
              GetObjectProxy(ibus::kDBusServiceName,
                             dbus::ObjectPath(ibus::kDBusObjectPath)))
      .WillOnce(Return(mock_dbus_proxy.get()));
  EXPECT_CALL(*mock_dbus_proxy, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&mock_get_name_owner_method_call,
                       &MockGetNameOwnerMethodCallHandler::Run));
  NameOwnerChangedHandler handler;
  EXPECT_CALL(*mock_dbus_proxy, ConnectToSignal(
      ibus::kDBusInterface,
      ibus::kNameOwnerChangedSignal,
      _,
      _)).WillOnce(Invoke(&handler,
                          &NameOwnerChangedHandler::OnConnectToSignal));
  client_->InitializeAsync(base::Bind(&base::DoNothing));

  // Passing empty string means there is no owner, thus ibus-config daemon is
  // not ready.
  mock_get_name_owner_method_call.EmitReplyCallback("");

  // Make sure not crashing by function call without initialize.
  const bool value = true;
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _)).Times(0);
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).Times(0);
  client_->SetBoolValue(kKey, kSection, value,
                        base::Bind(&MockErrorCallback::Run,
                                   base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, IBusConfigDaemon_SlowInitializeTest) {
  MockGetNameOwnerMethodCallHandler mock_get_name_owner_method_call;

  EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::config::kServiceName,
                                         dbus::ObjectPath(
                                             ibus::config::kServicePath)))
      .WillOnce(Return(mock_proxy_.get()));

  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy
      = new dbus::MockObjectProxy(mock_bus_.get(),
                                  ibus::kDBusServiceName,
                                  dbus::ObjectPath(ibus::kDBusObjectPath));
  EXPECT_CALL(*mock_bus_,
              GetObjectProxy(ibus::kDBusServiceName,
                             dbus::ObjectPath(ibus::kDBusObjectPath)))
      .WillOnce(Return(mock_dbus_proxy.get()));
  EXPECT_CALL(*mock_dbus_proxy, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&mock_get_name_owner_method_call,
                       &MockGetNameOwnerMethodCallHandler::Run));
  NameOwnerChangedHandler name_owner_changed_handler;
  EXPECT_CALL(*mock_dbus_proxy, ConnectToSignal(
      ibus::kDBusInterface,
      ibus::kNameOwnerChangedSignal,
      _,
      _)).WillOnce(Invoke(&name_owner_changed_handler,
                          &NameOwnerChangedHandler::OnConnectToSignal));
  client_->InitializeAsync(base::Bind(&base::DoNothing));

  // Passing empty string means there is no owner, thus ibus-config daemon is
  // not ready.
  mock_get_name_owner_method_call.EmitReplyCallback("");

  // Fire NameOwnerChanged signal to emulate ibus-config daemon was acquired
  // well-known name.
  name_owner_changed_handler.InvokeSignal(ibus::config::kServiceName,
                                          "",
                                          ":0.1");

  // Make sure it is possible to emit method calls.
  const bool value = true;
  SetBoolValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run());

  // Call SetBoolValue.
  client_->SetBoolValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));
}

TEST_F(IBusConfigClientTest, IBusConfigDaemon_ShutdownTest) {
  MockGetNameOwnerMethodCallHandler mock_get_name_owner_method_call;

  EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::config::kServiceName,
                                         dbus::ObjectPath(
                                             ibus::config::kServicePath)))
      .WillRepeatedly(Return(mock_proxy_.get()));

  scoped_refptr<dbus::MockObjectProxy> mock_dbus_proxy
      = new dbus::MockObjectProxy(mock_bus_.get(),
                                  ibus::kDBusServiceName,
                                  dbus::ObjectPath(ibus::kDBusObjectPath));
  EXPECT_CALL(*mock_bus_,
              GetObjectProxy(ibus::kDBusServiceName,
                             dbus::ObjectPath(ibus::kDBusObjectPath)))
      .WillOnce(Return(mock_dbus_proxy.get()));
  EXPECT_CALL(*mock_dbus_proxy, CallMethodWithErrorCallback(_, _, _, _))
      .WillOnce(Invoke(&mock_get_name_owner_method_call,
                       &MockGetNameOwnerMethodCallHandler::Run));
  NameOwnerChangedHandler name_owner_changed_handler;
  EXPECT_CALL(*mock_dbus_proxy, ConnectToSignal(
      ibus::kDBusInterface,
      ibus::kNameOwnerChangedSignal,
      _,
      _)).WillOnce(Invoke(&name_owner_changed_handler,
                          &NameOwnerChangedHandler::OnConnectToSignal));
  client_->InitializeAsync(base::Bind(&base::DoNothing));

  const bool value = true;
  SetBoolValueHandler handler(kSection, kKey, value, HANDLER_FAIL);
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillRepeatedly(Invoke(&handler, &SetValueVerifierBase::Run));
  MockErrorCallback error_callback;
  EXPECT_CALL(error_callback, Run()).WillRepeatedly(Return());

  // Initialize succeeded sucessfully.
  mock_get_name_owner_method_call.EmitReplyCallback(":0.2");

  // Call SetBoolValue.
  client_->SetBoolValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));

  // Fire NameOwnerChanged signal to emulate shutting down the ibus-config
  // daemon.
  name_owner_changed_handler.InvokeSignal(ibus::config::kServiceName,
                                          ":0.1",
                                          "");

  // Make sure not crashing on emitting method call.
  client_->SetBoolValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));

  // Fire NameOwnerChanged signal to emulate that ibus-daemon is revived.
  name_owner_changed_handler.InvokeSignal(ibus::config::kServiceName,
                                          "",
                                          ":0.2");

  // Make sure it is possible to emit method calls.
  client_->SetBoolValue(kKey, kSection, value,
                       base::Bind(&MockErrorCallback::Run,
                                  base::Unretained(&error_callback)));
}

}  // namespace chromeos
