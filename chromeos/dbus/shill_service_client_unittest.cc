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

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

const char kExampleServicePath[] = "/foo/bar";

}  // namespace

class ShillServiceClientTest : public ShillClientUnittestBase {
 public:
  ShillServiceClientTest()
      : ShillClientUnittestBase(shill::kFlimflamServiceInterface,
                                   dbus::ObjectPath(kExampleServicePath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillServiceClient::Create());
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunUntilIdle();
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
  dbus::Signal signal(shill::kFlimflamServiceInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kSignalStrengthProperty);
  writer.AppendVariantOfByte(kValue);

  // Set expectations.
  const base::FundamentalValue value(kValue);
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(
                  shill::kSignalStrengthProperty,
                  ValueEq(ByRef(value)))).Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(
      dbus::ObjectPath(kExampleServicePath),
      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(
      dbus::ObjectPath(kExampleServicePath),
      &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
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
  entry_writer.AppendString(shill::kSignalStrengthProperty);
  entry_writer.AppendVariantOfByte(kValue);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(shill::kSignalStrengthProperty,
                                base::Value::CreateIntegerValue(kValue));
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kExampleServicePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, SetProperty) {
  const char kValue[] = "passphrase";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  const base::StringValue value(kValue);
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(&ExpectStringAndValueArguments,
                                  shill::kPassphraseProperty,
                                  &value),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->SetProperty(dbus::ObjectPath(kExampleServicePath),
                       shill::kPassphraseProperty,
                       value,
                       mock_closure.GetCallback(),
                       mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, SetProperties) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  scoped_ptr<base::DictionaryValue> arg(CreateExampleServiceProperties());
  PrepareForMethodCall(shill::kSetPropertiesFunction,
                       base::Bind(&ExpectDictionaryValueArgument, arg.get()),
                       response.get());

  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->SetProperties(dbus::ObjectPath(kExampleServicePath),
                         *arg,
                         mock_closure.GetCallback(),
                         mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ClearProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kClearPropertyFunction,
                       base::Bind(&ExpectStringArgument,
                                  shill::kPassphraseProperty),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->ClearProperty(dbus::ObjectPath(kExampleServicePath),
                         shill::kPassphraseProperty,
                         mock_closure.GetCallback(),
                         mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ClearProperties) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("b", &array_writer);
  array_writer.AppendBool(true);
  array_writer.AppendBool(true);
  writer.CloseContainer(&array_writer);

  // Set expectations.
  std::vector<std::string> keys;
  keys.push_back(shill::kPassphraseProperty);
  keys.push_back(shill::kSignalStrengthProperty);
  PrepareForMethodCall(shill::kClearPropertiesFunction,
                       base::Bind(&ExpectArrayOfStringsArgument, keys),
                       response.get());
  // Call method.
  MockListValueCallback mock_list_value_callback;
  MockErrorCallback mock_error_callback;
  client_->ClearProperties(dbus::ObjectPath(kExampleServicePath),
                           keys,
                           mock_list_value_callback.GetCallback(),
                           mock_error_callback.GetCallback());
  EXPECT_CALL(mock_list_value_callback, Run(_)).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Connect) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  PrepareForMethodCall(shill::kConnectFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  // Call method.
  client_->Connect(dbus::ObjectPath(kExampleServicePath),
                   mock_closure.GetCallback(),
                   mock_error_callback.GetCallback());

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Disconnect) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kDisconnectFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->Disconnect(dbus::ObjectPath(kExampleServicePath),
                      mock_closure.GetCallback(),
                      mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, Remove) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kRemoveServiceFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->Remove(dbus::ObjectPath(kExampleServicePath),
                  mock_closure.GetCallback(),
                  mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillServiceClientTest, ActivateCellularModem) {
  const char kCarrier[] = "carrier";
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Set expectations.
  PrepareForMethodCall(shill::kActivateCellularModemFunction,
                       base::Bind(&ExpectStringArgument, kCarrier),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->ActivateCellularModem(dbus::ObjectPath(kExampleServicePath),
                                 kCarrier,
                                 mock_closure.GetCallback(),
                                 mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
