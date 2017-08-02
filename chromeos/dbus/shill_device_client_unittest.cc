// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "chromeos/dbus/shill_device_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

const char kExampleDevicePath[] = "/foo/bar";

// Expects the reader to have a string and a bool.
void ExpectStringAndBoolArguments(const std::string& expected_string,
                                  bool expected_bool,
                                  dbus::MessageReader* reader) {
  std::string arg1;
  ASSERT_TRUE(reader->PopString(&arg1));
  EXPECT_EQ(expected_string, arg1);
  bool arg2 = false;
  ASSERT_TRUE(reader->PopBool(&arg2));
  EXPECT_EQ(expected_bool, arg2);
  EXPECT_FALSE(reader->HasMoreData());
}

// Expects the reader to have two strings.
void ExpectTwoStringArguments(const std::string& expected_string1,
                              const std::string& expected_string2,
                              dbus::MessageReader* reader) {
  std::string arg1;
  ASSERT_TRUE(reader->PopString(&arg1));
  EXPECT_EQ(expected_string1, arg1);
  std::string arg2;
  ASSERT_TRUE(reader->PopString(&arg2));
  EXPECT_EQ(expected_string2, arg2);
  EXPECT_FALSE(reader->HasMoreData());
}

}  // namespace

class ShillDeviceClientTest : public ShillClientUnittestBase {
 public:
  ShillDeviceClientTest()
      : ShillClientUnittestBase(shill::kFlimflamDeviceInterface,
                                   dbus::ObjectPath(kExampleDevicePath)) {
  }

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillDeviceClient::Create());
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { ShillClientUnittestBase::TearDown(); }

 protected:
  std::unique_ptr<ShillDeviceClient> client_;
};

TEST_F(ShillDeviceClientTest, PropertyChanged) {
  const bool kValue = true;
  // Create a signal.
  dbus::Signal signal(shill::kFlimflamDeviceInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kCellularAllowRoamingProperty);
  writer.AppendVariantOfBool(kValue);

  // Set expectations.
  const base::Value value(kValue);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kCellularAllowRoamingProperty,
                                ValueEq(ByRef(value)))).Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(
      dbus::ObjectPath(kExampleDevicePath),
      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(
      dbus::ObjectPath(kExampleDevicePath),
      &observer);

  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kCellularAllowRoamingProperty,
                                ValueEq(ByRef(value)))).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillDeviceClientTest, GetProperties) {
  const bool kValue = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kCellularAllowRoamingProperty);
  entry_writer.AppendVariantOfBool(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::DictionaryValue value;
  value.SetKey(shill::kCellularAllowRoamingProperty, base::Value(kValue));
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleDevicePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, ProposeScan) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kProposeScanFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->ProposeScan(dbus::ObjectPath(kExampleDevicePath),
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, SetProperty) {
  const bool kValue = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::Value value(kValue);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  shill::kCellularAllowRoamingProperty,
                                  &value),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  client_->SetProperty(dbus::ObjectPath(kExampleDevicePath),
                       shill::kCellularAllowRoamingProperty, value,
                       mock_closure.Get(), mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, ClearProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  shill::kCellularAllowRoamingProperty),
                       response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleDevicePath),
                         shill::kCellularAllowRoamingProperty,
                         base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, AddIPConfig) {
  const dbus::ObjectPath expected_result("/result/path");
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(expected_result);

  // Set expectations.
  PrepareForMethodCall(shill::kAddIPConfigFunction,
                       base::Bind(&ExpectStringArgument, shill::kTypeDHCP),
                       response.get());
  // Call method.
  client_->AddIPConfig(dbus::ObjectPath(kExampleDevicePath),
                       shill::kTypeDHCP,
                       base::Bind(&ExpectObjectPathResult, expected_result));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, RequirePin) {
  const char kPin[] = "123456";
  const bool kRequired = true;
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kRequirePinFunction,
                       base::Bind(&ExpectStringAndBoolArguments,
                                  kPin,
                                  kRequired),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);
  // Call method.
  client_->RequirePin(dbus::ObjectPath(kExampleDevicePath), kPin, kRequired,
                      mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, EnterPin) {
  const char kPin[] = "123456";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kEnterPinFunction,
                       base::Bind(&ExpectStringArgument,
                                  kPin),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Call method.
  client_->EnterPin(dbus::ObjectPath(kExampleDevicePath), kPin,
                    mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, UnblockPin) {
  const char kPuk[] = "987654";
  const char kPin[] = "123456";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kUnblockPinFunction,
                       base::Bind(&ExpectTwoStringArguments, kPuk, kPin),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Call method.
  client_->UnblockPin(dbus::ObjectPath(kExampleDevicePath), kPuk, kPin,
                      mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, ChangePin) {
  const char kOldPin[] = "123456";
  const char kNewPin[] = "234567";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kChangePinFunction,
                       base::Bind(&ExpectTwoStringArguments,
                                  kOldPin,
                                  kNewPin),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Call method.
  client_->ChangePin(dbus::ObjectPath(kExampleDevicePath), kOldPin, kNewPin,
                     mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, Register) {
  const char kNetworkId[] = "networkid";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kRegisterFunction,
                       base::Bind(&ExpectStringArgument, kNetworkId),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Call method.
  client_->Register(dbus::ObjectPath(kExampleDevicePath), kNetworkId,
                    mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, SetCarrier) {
  const char kCarrier[] = "carrier";
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kSetCarrierFunction,
                       base::Bind(&ExpectStringArgument, kCarrier),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  // Call method.
  client_->SetCarrier(dbus::ObjectPath(kExampleDevicePath), kCarrier,
                      mock_closure.Get(), mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillDeviceClientTest, Reset) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillDeviceClient::ErrorCallback> mock_error_callback;
  PrepareForMethodCall(shill::kResetFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  // Call method.
  client_->Reset(dbus::ObjectPath(kExampleDevicePath), mock_closure.Get(),
                 mock_error_callback.Get());
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
