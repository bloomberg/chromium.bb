// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "chromeos/dbus/shill_ipconfig_client.h"
#include "dbus/message.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kExampleIPConfigPath[] = "/foo/bar";

}  // namespace

class ShillIPConfigClientTest : public ShillClientUnittestBase {
 public:
  ShillIPConfigClientTest()
      : ShillClientUnittestBase(
          flimflam::kFlimflamIPConfigInterface,
          dbus::ObjectPath(kExampleIPConfigPath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillIPConfigClient::Create(
        REAL_DBUS_CLIENT_IMPLEMENTATION, mock_bus_));
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    ShillClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<ShillIPConfigClient> client_;
};

TEST_F(ShillIPConfigClientTest, PropertyChanged) {
  // Create a signal.
  const base::FundamentalValue kConnected(true);
  dbus::Signal signal(flimflam::kFlimflamIPConfigInterface,
                      flimflam::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(flimflam::kConnectedProperty);
  dbus::AppendBasicTypeValueDataAsVariant(&writer, kConnected);

  // Set expectations.
  client_->SetPropertyChangedHandler(dbus::ObjectPath(kExampleIPConfigPath),
                                     base::Bind(&ExpectPropertyChanged,
                                                flimflam::kConnectedProperty,
                                                &kConnected));
  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Reset the handler.
  client_->ResetPropertyChangedHandler(dbus::ObjectPath(kExampleIPConfigPath));
}

TEST_F(ShillIPConfigClientTest, GetProperties) {
  const char kAddress[] = "address";
  const int32 kMtu = 68;

  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  // Append address.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kAddressProperty);
  entry_writer.AppendVariantOfString(kAddress);
  array_writer.CloseContainer(&entry_writer);
  // Append MTU.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kMtuProperty);
  entry_writer.AppendVariantOfInt32(kMtu);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kAddressProperty,
                                base::Value::CreateStringValue(kAddress));
  value.SetWithoutPathExpansion(flimflam::kMtuProperty,
                                base::Value::CreateIntegerValue(kMtu));

  // Set expectations.
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleIPConfigPath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillIPConfigClientTest, CallGetPropertiesAndBlock) {
  const char kAddress[] = "address";
  const int32 kMtu = 68;

  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  // Append address.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kAddressProperty);
  entry_writer.AppendVariantOfString(kAddress);
  array_writer.CloseContainer(&entry_writer);
  // Append MTU.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kMtuProperty);
  entry_writer.AppendVariantOfInt32(kMtu);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kAddressProperty,
                                base::Value::CreateStringValue(kAddress));
  value.SetWithoutPathExpansion(flimflam::kMtuProperty,
                                base::Value::CreateIntegerValue(kMtu));
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  scoped_ptr<base::DictionaryValue> result(
      client_->CallGetPropertiesAndBlock(
          dbus::ObjectPath(kExampleIPConfigPath)));

  ASSERT_TRUE(result.get());
  EXPECT_TRUE(result->Equals(&value));
}

TEST_F(ShillIPConfigClientTest, SetProperty) {
  const char kAddress[] = "address";

  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::StringValue value(kAddress);
  PrepareForMethodCall(flimflam::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  flimflam::kAddressProperty,
                                  &value),
                       response.get());
  // Call method.
  client_->SetProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       flimflam::kAddressProperty,
                       value,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillIPConfigClientTest, ClearProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  flimflam::kAddressProperty),
                       response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       flimflam::kAddressProperty,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillIPConfigClientTest, Remove) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kRemoveConfigFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->Remove(dbus::ObjectPath(kExampleIPConfigPath),
                  base::Bind(&ExpectNoResultValue));

  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillIPConfigClientTest, CallRemoveAndBlock) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(flimflam::kRemoveConfigFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  EXPECT_TRUE(client_->CallRemoveAndBlock(
      dbus::ObjectPath(kExampleIPConfigPath)));
}

}  // namespace chromeos
