// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kExampleServicePath[] = "/foo/bar";

}  // namespace

class ShillServiceClientTest : public ShillClientUnittestBase {
 public:
  ShillServiceClientTest()
      : ShillClientUnittestBase(flimflam::kFlimflamServiceInterface,
                                   dbus::ObjectPath(kExampleServicePath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillServiceClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                               mock_bus_));
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    ShillClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<ShillServiceClient> client_;
};

TEST_F(ShillServiceClientTest, PropertyChanged) {
  const int kValue = 42;
  // Create a signal.
  dbus::Signal signal(flimflam::kFlimflamServiceInterface,
                      flimflam::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(flimflam::kSignalStrengthProperty);
  writer.AppendVariantOfByte(kValue);

  // Set expectations.
  const base::FundamentalValue value(kValue);
  client_->SetPropertyChangedHandler(
      dbus::ObjectPath(kExampleServicePath),
      base::Bind(&ExpectPropertyChanged,
                 flimflam::kSignalStrengthProperty,
                 &value));
  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Reset the handler.
  client_->ResetPropertyChangedHandler(dbus::ObjectPath(kExampleServicePath));
}

TEST_F(ShillServiceClientTest, GetProperties) {
  const int kValue = 42;
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kSignalStrengthProperty);
  entry_writer.AppendVariantOfByte(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kSignalStrengthProperty,
                                base::Value::CreateIntegerValue(kValue));
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleServicePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, SetProperty) {
  const char kValue[] = "passphrase";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::StringValue value(kValue);
  PrepareForMethodCall(flimflam::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  flimflam::kPassphraseProperty,
                                  &value),
                       response.get());
  // Call method.
  client_->SetProperty(dbus::ObjectPath(kExampleServicePath),
                       flimflam::kPassphraseProperty,
                       value,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, ClearProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  flimflam::kPassphraseProperty),
                       response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleServicePath),
                         flimflam::kPassphraseProperty,
                         base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, Connect) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  PrepareForMethodCall(flimflam::kConnectFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  // Call method.
  client_->Connect(dbus::ObjectPath(kExampleServicePath),
                   mock_closure.GetCallback(),
                   mock_error_callback.GetCallback());

  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, Disconnect) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kDisconnectFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->Disconnect(dbus::ObjectPath(kExampleServicePath),
                   base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, Remove) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kRemoveServiceFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->Remove(dbus::ObjectPath(kExampleServicePath),
                  base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, ActivateCellularModem) {
  const char kCarrier[] = "carrier";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kActivateCellularModemFunction,
                       base::Bind(&ExpectStringArgument, kCarrier),
                       response.get());
  // Call method.
  client_->ActivateCellularModem(dbus::ObjectPath(kExampleServicePath),
                                 kCarrier,
                                 base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillServiceClientTest, CallActivateCellularModemAndBlock) {
  const char kCarrier[] = "carrier";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kActivateCellularModemFunction,
                       base::Bind(&ExpectStringArgument, kCarrier),
                       response.get());
  // Call method.
  const bool result = client_->CallActivateCellularModemAndBlock(
      dbus::ObjectPath(kExampleServicePath), kCarrier);
  EXPECT_TRUE(result);
}

}  // namespace chromeos
