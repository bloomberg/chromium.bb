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

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

const char kExampleIPConfigPath[] = "/foo/bar";

}  // namespace

class ShillIPConfigClientTest : public ShillClientUnittestBase {
 public:
  ShillIPConfigClientTest()
      : ShillClientUnittestBase(
          shill::kFlimflamIPConfigInterface,
          dbus::ObjectPath(kExampleIPConfigPath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillIPConfigClient::Create());
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunUntilIdle();
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
  dbus::Signal signal(shill::kFlimflamIPConfigInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kConnectedProperty);
  dbus::AppendBasicTypeValueDataAsVariant(&writer, kConnected);

  // Set expectations.
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer, OnPropertyChanged(shill::kConnectedProperty,
                                          ValueEq(ByRef(kConnected)))).Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(
      dbus::ObjectPath(kExampleIPConfigPath),
      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(
      dbus::ObjectPath(kExampleIPConfigPath),
      &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
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
  entry_writer.AppendString(shill::kAddressProperty);
  entry_writer.AppendVariantOfString(kAddress);
  array_writer.CloseContainer(&entry_writer);
  // Append MTU.
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kMtuProperty);
  entry_writer.AppendVariantOfInt32(kMtu);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(shill::kAddressProperty,
                                new base::StringValue(kAddress));
  value.SetWithoutPathExpansion(shill::kMtuProperty,
                                new base::FundamentalValue(kMtu));

  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleIPConfigPath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, SetProperty) {
  const char kAddress[] = "address";

  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  base::StringValue value(kAddress);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  shill::kAddressProperty,
                                  &value),
                       response.get());
  // Call method.
  client_->SetProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       shill::kAddressProperty,
                       value,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, ClearProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  shill::kAddressProperty),
                       response.get());
  // Call method.
  client_->ClearProperty(dbus::ObjectPath(kExampleIPConfigPath),
                       shill::kAddressProperty,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillIPConfigClientTest, Remove) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveConfigFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->Remove(dbus::ObjectPath(kExampleIPConfigPath),
                  base::Bind(&ExpectNoResultValue));

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
