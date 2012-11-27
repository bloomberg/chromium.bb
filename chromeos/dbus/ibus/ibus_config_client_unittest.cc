// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/ibus/ibus_config_client.h"

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
    EXPECT_CALL(*mock_bus_, GetObjectProxy(ibus::kServiceName,
                                           dbus::ObjectPath(
                                               ibus::config::kServicePath)))
        .WillOnce(Return(mock_proxy_.get()));

    EXPECT_CALL(*mock_bus_, ShutdownAndBlock());
    client_.reset(IBusConfigClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                           mock_bus_));
  }

  virtual void TearDown() OVERRIDE {
    mock_bus_->ShutdownAndBlock();
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

}  // namespace chromeos
