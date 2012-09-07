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
      : ShillClientUnittestBase(flimflam::kFlimflamProfileInterface,
                                   dbus::ObjectPath(kDefaultProfilePath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillProfileClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                                mock_bus_));
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    ShillClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<ShillProfileClient> client_;
};

TEST_F(ShillProfileClientTest, PropertyChanged) {
  // Create a signal.
  dbus::Signal signal(flimflam::kFlimflamProfileInterface,
                      flimflam::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(flimflam::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&writer,
                                std::vector<std::string>(1, kExampleEntryPath));

  // Set expectations.
  base::ListValue value;
  value.Append(base::Value::CreateStringValue(kExampleEntryPath));

  client_->SetPropertyChangedHandler(dbus::ObjectPath(kDefaultProfilePath),
                                     base::Bind(&ExpectPropertyChanged,
                                                flimflam::kEntriesProperty,
                                                &value));
  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Reset the handler.
  client_->ResetPropertyChangedHandler(dbus::ObjectPath(kDefaultProfilePath));
}

TEST_F(ShillProfileClientTest, GetProperties) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kEntriesProperty);
  AppendVariantOfArrayOfStrings(&entry_writer,
                                std::vector<std::string>(1, kExampleEntryPath));
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::ListValue* entries = new base::ListValue;
  entries->Append(base::Value::CreateStringValue(kExampleEntryPath));
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kEntriesProperty, entries);
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(dbus::ObjectPath(kDefaultProfilePath),
                         base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillProfileClientTest, GetEntry) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kTypeProperty);
  entry_writer.AppendVariantOfString(flimflam::kTypeWifi);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(flimflam::kTypeWifi));
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetEntryFunction,
                       base::Bind(&ExpectStringArgument, kExampleEntryPath),
                       response.get());
  // Call method.
  client_->GetEntry(dbus::ObjectPath(kDefaultProfilePath),
                    kExampleEntryPath,
                    base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillProfileClientTest, DeleteEntry) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kOfflineModeProperty,
                                base::Value::CreateBooleanValue(true));
  // Set expectations.
  PrepareForMethodCall(flimflam::kDeleteEntryFunction,
                       base::Bind(&ExpectStringArgument, kExampleEntryPath),
                       response.get());
  // Call method.
  client_->DeleteEntry(dbus::ObjectPath(kDefaultProfilePath),
                       kExampleEntryPath,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

}  // namespace chromeos
