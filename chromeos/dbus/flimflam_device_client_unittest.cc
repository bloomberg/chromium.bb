// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/flimflam_client_unittest_base.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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

class FlimflamDeviceClientTest : public FlimflamClientUnittestBase {
 public:
  FlimflamDeviceClientTest()
      : FlimflamClientUnittestBase(flimflam::kFlimflamDeviceInterface,
                                   dbus::ObjectPath(kExampleDevicePath)) {
  }

  virtual void SetUp() {
    FlimflamClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(FlimflamDeviceClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                               mock_bus_));
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    FlimflamClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<FlimflamDeviceClient> client_;
};

TEST_F(FlimflamDeviceClientTest, PropertyChanged) {
  const bool kValue = true;
  // Create a signal.
  dbus::Signal signal(flimflam::kFlimflamDeviceInterface,
                      flimflam::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(flimflam::kCellularAllowRoamingProperty);
  writer.AppendVariantOfBool(kValue);

  // Set expectations.
  const base::FundamentalValue value(kValue);
  client_->SetPropertyChangedHandler(
      dbus::ObjectPath(kExampleDevicePath),
      base::Bind(&ExpectPropertyChanged,
                 flimflam::kCellularAllowRoamingProperty,
                 &value));
  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Reset the handler.
  client_->ResetPropertyChangedHandler(dbus::ObjectPath(kExampleDevicePath));
}

TEST_F(FlimflamDeviceClientTest, GetProperties) {
  const bool kValue = true;
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kCellularAllowRoamingProperty);
  entry_writer.AppendVariantOfBool(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kCellularAllowRoamingProperty,
                                base::Value::CreateBooleanValue(kValue));
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleDevicePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, ProposeScan) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kProposeScanFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->ProposeScan(dbus::ObjectPath(kExampleDevicePath),
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, SetProperty) {
  const bool kValue = true;
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::FundamentalValue value(kValue);
  PrepareForMethodCall(flimflam::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  flimflam::kCellularAllowRoamingProperty,
                                  &value),
                       response.get());
  // Call method.
  client_->SetProperty(dbus::ObjectPath(kExampleDevicePath),
                       flimflam::kCellularAllowRoamingProperty,
                       value,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, ClearProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  flimflam::kCellularAllowRoamingProperty),
                       response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleDevicePath),
                         flimflam::kCellularAllowRoamingProperty,
                         base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, RequirePin) {
  const char kPin[] = "123456";
  const bool kRequired = true;
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kRequirePinFunction,
                       base::Bind(&ExpectStringAndBoolArguments,
                                  kPin,
                                  kRequired),
                       response.get());
  // Call method.
  client_->RequirePin(dbus::ObjectPath(kExampleDevicePath),
                      kPin,
                      kRequired,
                      base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, EnterPin) {
  const char kPin[] = "123456";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kEnterPinFunction,
                       base::Bind(&ExpectStringArgument,
                                  kPin),
                       response.get());
  // Call method.
  client_->EnterPin(dbus::ObjectPath(kExampleDevicePath),
                    kPin,
                    base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, UnblockPin) {
  const char kPuk[] = "987654";
  const char kPin[] = "123456";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kUnblockPinFunction,
                       base::Bind(&ExpectTwoStringArguments, kPuk, kPin),
                       response.get());
  // Call method.
  client_->UnblockPin(dbus::ObjectPath(kExampleDevicePath),
                      kPuk,
                      kPin,
                      base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, ChangePin) {
  const char kOldPin[] = "123456";
  const char kNewPin[] = "234567";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kChangePinFunction,
                       base::Bind(&ExpectTwoStringArguments,
                                  kOldPin,
                                  kNewPin),
                       response.get());
  // Call method.
  client_->ChangePin(dbus::ObjectPath(kExampleDevicePath),
                     kOldPin,
                     kNewPin,
                     base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(FlimflamDeviceClientTest, Register) {
  const char kNetworkId[] = "networkid";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kRegisterFunction,
                       base::Bind(&ExpectStringArgument, kNetworkId),
                       response.get());
  // Call method.
  client_->Register(dbus::ObjectPath(kExampleDevicePath),
                    kNetworkId,
                    base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

}  // namespace chromeos
