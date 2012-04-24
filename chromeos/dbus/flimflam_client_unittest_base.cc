// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/flimflam_client_unittest_base.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "dbus/message.h"
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

// Runs the given task.  This function is used to implement the mock bus.
void RunTask(const tracked_objects::Location& from_here,
             const base::Closure& task) {
  task.Run();
}

}  // namespace

FlimflamClientUnittestBase::MockClosure::MockClosure() {}

FlimflamClientUnittestBase::MockClosure::~MockClosure() {}

base::Closure FlimflamClientUnittestBase::MockClosure::GetCallback() {
  return base::Bind(&MockClosure::Run, base::Unretained(this));
}


FlimflamClientUnittestBase::MockErrorCallback::MockErrorCallback() {}

FlimflamClientUnittestBase::MockErrorCallback::~MockErrorCallback() {}

FlimflamClientHelper::ErrorCallback
FlimflamClientUnittestBase::MockErrorCallback::GetCallback() {
  return base::Bind(&MockErrorCallback::Run, base::Unretained(this));
}


FlimflamClientUnittestBase::FlimflamClientUnittestBase(
    const std::string& interface_name,
    const dbus::ObjectPath& object_path)
    : interface_name_(interface_name),
      object_path_(object_path),
      response_(NULL) {
}

FlimflamClientUnittestBase::~FlimflamClientUnittestBase() {
}

void FlimflamClientUnittestBase::SetUp() {
  // Create a mock bus.
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SYSTEM;
  mock_bus_ = new dbus::MockBus(options);

  // Create a mock proxy.
  mock_proxy_ = new dbus::MockObjectProxy(
      mock_bus_.get(),
      flimflam::kFlimflamServiceName,
      object_path_);

  // Set an expectation so mock_proxy's CallMethodAndBlock() will use
  // OnCallMethodAndBlock() to return responses.
  EXPECT_CALL(*mock_proxy_, CallMethodAndBlock(_, _))
      .WillRepeatedly(Invoke(
          this, &FlimflamClientUnittestBase::OnCallMethodAndBlock));

  // Set an expectation so mock_proxy's CallMethod() will use OnCallMethod()
  // to return responses.
  EXPECT_CALL(*mock_proxy_, CallMethod(_, _, _))
      .WillRepeatedly(Invoke(this, &FlimflamClientUnittestBase::OnCallMethod));

  // Set an expectation so mock_proxy's CallMethodWithErrorCallback() will use
  // OnCallMethodWithErrorCallback() to return responses.
  EXPECT_CALL(*mock_proxy_, CallMethodWithErrorCallback(_, _, _, _))
      .WillRepeatedly(Invoke(
          this, &FlimflamClientUnittestBase::OnCallMethodWithErrorCallback));

  // Set an expectation so mock_proxy's ConnectToSignal() will use
  // OnConnectToSignal() to run the callback.
  EXPECT_CALL(*mock_proxy_, ConnectToSignal(
      interface_name_,
      flimflam::kMonitorPropertyChanged, _, _))
      .WillRepeatedly(Invoke(this,
                             &FlimflamClientUnittestBase::OnConnectToSignal));

  // Set an expectation so mock_bus's GetObjectProxy() for the given
  // service name and the object path will return mock_proxy_.
  EXPECT_CALL(*mock_bus_, GetObjectProxy(flimflam::kFlimflamServiceName,
                                         object_path_))
      .WillOnce(Return(mock_proxy_.get()));

  // Set an expectation so mock_bus's PostTaskToDBusThread() will run the
  // given task.
  EXPECT_CALL(*mock_bus_, PostTaskToDBusThread(_, _))
      .WillRepeatedly(Invoke(&RunTask));

  // ShutdownAndBlock() will be called in TearDown().
  EXPECT_CALL(*mock_bus_, ShutdownAndBlock()).WillOnce(Return());
}

void FlimflamClientUnittestBase::TearDown() {
  mock_bus_->ShutdownAndBlock();
}

void FlimflamClientUnittestBase::PrepareForMethodCall(
    const std::string& method_name,
    const ArgumentCheckCallback& argument_checker,
    dbus::Response* response) {
  expected_method_name_ = method_name;
  argument_checker_ = argument_checker;
  response_ = response;
}

void FlimflamClientUnittestBase::SendPropertyChangedSignal(
    dbus::Signal* signal) {
  ASSERT_FALSE(property_changed_handler_.is_null());
  property_changed_handler_.Run(signal);
}

// static
void FlimflamClientUnittestBase::ExpectPropertyChanged(
    const std::string& expected_name,
    const base::Value* expected_value,
    const std::string& name,
    const base::Value& value) {
  EXPECT_EQ(expected_name, name);
  EXPECT_TRUE(expected_value->Equals(&value));
}

// static
void FlimflamClientUnittestBase::ExpectNoArgument(dbus::MessageReader* reader) {
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void FlimflamClientUnittestBase::ExpectStringArgument(
    const std::string& expected_string,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void FlimflamClientUnittestBase::ExpectValueArgument(
    const base::Value* expected_value,
    dbus::MessageReader* reader) {
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(reader));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_value));
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void FlimflamClientUnittestBase::ExpectStringAndValueArguments(
    const std::string& expected_string,
    const base::Value* expected_value,
    dbus::MessageReader* reader) {
  std::string str;
  ASSERT_TRUE(reader->PopString(&str));
  EXPECT_EQ(expected_string, str);
  scoped_ptr<base::Value> value(dbus::PopDataAsValue(reader));
  ASSERT_TRUE(value.get());
  EXPECT_TRUE(value->Equals(expected_value));
  EXPECT_FALSE(reader->HasMoreData());
}

// static
void FlimflamClientUnittestBase::ExpectNoResultValue(
    DBusMethodCallStatus call_status) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
}

// static
void FlimflamClientUnittestBase::ExpectObjectPathResult(
    const dbus::ObjectPath& expected_result,
    DBusMethodCallStatus call_status,
    const dbus::ObjectPath& result) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
  EXPECT_EQ(expected_result, result);
}

// static
void FlimflamClientUnittestBase::ExpectDictionaryValueResult(
    const base::DictionaryValue* expected_result,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& result) {
  EXPECT_EQ(DBUS_METHOD_CALL_SUCCESS, call_status);
  std::string expected_result_string;
  base::JSONWriter::Write(expected_result, &expected_result_string);
  std::string result_string;
  base::JSONWriter::Write(&result, &result_string);
  EXPECT_EQ(expected_result_string, result_string);
}

void FlimflamClientUnittestBase::OnConnectToSignal(
    const std::string& interface_name,
    const std::string& signal_name,
    const dbus::ObjectProxy::SignalCallback& signal_callback,
    const dbus::ObjectProxy::OnConnectedCallback& on_connected_callback) {
  property_changed_handler_ = signal_callback;
  const bool success = true;
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(on_connected_callback,
                                    interface_name,
                                    signal_name,
                                    success));
}

void FlimflamClientUnittestBase::OnCallMethod(
    dbus::MethodCall* method_call,
    int timeout_ms,
    const dbus::ObjectProxy::ResponseCallback& response_callback) {
  EXPECT_EQ(interface_name_, method_call->GetInterface());
  EXPECT_EQ(expected_method_name_, method_call->GetMember());
  dbus::MessageReader reader(method_call);
  argument_checker_.Run(&reader);
  message_loop_.PostTask(FROM_HERE,
                         base::Bind(response_callback, response_));
}

void FlimflamClientUnittestBase::OnCallMethodWithErrorCallback(
    dbus::MethodCall* method_call,
    int timeout_ms,
    const dbus::ObjectProxy::ResponseCallback& response_callback,
    const dbus::ObjectProxy::ErrorCallback& error_callback) {
  OnCallMethod(method_call, timeout_ms, response_callback);
}

dbus::Response* FlimflamClientUnittestBase::OnCallMethodAndBlock(
    dbus::MethodCall* method_call,
    int timeout_ms) {
  EXPECT_EQ(interface_name_, method_call->GetInterface());
  EXPECT_EQ(expected_method_name_, method_call->GetMember());
  dbus::MessageReader reader(method_call);
  argument_checker_.Run(&reader);
  return dbus::Response::FromRawMessage(
      dbus_message_copy(response_->raw_message()));
}

}  // namespace chromeos
