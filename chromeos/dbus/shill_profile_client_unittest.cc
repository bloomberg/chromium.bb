// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "chromeos/dbus/shill_profile_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;

namespace chromeos {

namespace {

const char kDefaultProfilePath[] = "/profile/default";
const char kExampleEntryPath[] = "example_entry_path";

void AppendVariantOfArrayOfStrings(dbus::MessageWriter* writer,
                                   const std::vector<std::string>& strings) {
  dbus::MessageWriter variant_writer(NULL);
  writer->OpenVariant("as", &variant_writer);
  variant_writer.AppendArrayOfStrings(strings);
  writer->CloseContainer(&variant_writer);
}

}  // namespace

class ShillProfileClientTest : public ShillClientUnittestBase {
 public:
  ShillProfileClientTest()
      : ShillClientUnittestBase(shill::kFlimflamProfileInterface,
                                   dbus::ObjectPath(kDefaultProfilePath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillProfileClient::Create());
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunUntilIdle();
  }

  virtual void TearDown() {
    ShillClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<ShillProfileClient> client_;
};

TEST_F(ShillProfileClientTest, PropertyChanged) {
  // Create a signal.
  dbus::Signal signal(shill::kFlimflamProfileInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&writer,
                                std::vector<std::string>(1, kExampleEntryPath));

  // Set expectations.
  base::ListValue value;
  value.Append(new base::StringValue(kExampleEntryPath));
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(
                  shill::kEntriesProperty,
                  ValueEq(value))).Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(
      dbus::ObjectPath(kDefaultProfilePath),
      &observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(
      dbus::ObjectPath(kDefaultProfilePath),
      &observer);

  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillProfileClientTest, GetProperties) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&entry_writer,
                                std::vector<std::string>(1, kExampleEntryPath));
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::ListValue* entries = new base::ListValue;
  entries->Append(new base::StringValue(kExampleEntryPath));
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(shill::kEntriesProperty, entries);
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  MockErrorCallback error_callback;
  client_->GetProperties(dbus::ObjectPath(kDefaultProfilePath),
                         base::Bind(&ExpectDictionaryValueResultWithoutStatus,
                                    &value),
                         error_callback.GetCallback());
  EXPECT_CALL(error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillProfileClientTest, GetEntry) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kTypeProperty);
  entry_writer.AppendVariantOfString(shill::kTypeWifi);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(shill::kTypeProperty,
                                new base::StringValue(shill::kTypeWifi));
  // Set expectations.
  PrepareForMethodCall(shill::kGetEntryFunction,
                       base::Bind(&ExpectStringArgument, kExampleEntryPath),
                       response.get());
  // Call method.
  MockErrorCallback error_callback;
  client_->GetEntry(dbus::ObjectPath(kDefaultProfilePath),
                    kExampleEntryPath,
                    base::Bind(&ExpectDictionaryValueResultWithoutStatus,
                               &value),
                    error_callback.GetCallback());
  EXPECT_CALL(error_callback, Run(_, _)).Times(0);
  // Run the message loop.
  message_loop_.RunUntilIdle();
}

TEST_F(ShillProfileClientTest, DeleteEntry) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(shill::kOfflineModeProperty,
                                new base::FundamentalValue(true));
  // Set expectations.
  PrepareForMethodCall(shill::kDeleteEntryFunction,
                       base::Bind(&ExpectStringArgument, kExampleEntryPath),
                       response.get());
  // Call method.
  MockClosure mock_closure;
  MockErrorCallback mock_error_callback;
  client_->DeleteEntry(dbus::ObjectPath(kDefaultProfilePath),
                       kExampleEntryPath,
                       mock_closure.GetCallback(),
                       mock_error_callback.GetCallback());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  message_loop_.RunUntilIdle();
}

}  // namespace chromeos
