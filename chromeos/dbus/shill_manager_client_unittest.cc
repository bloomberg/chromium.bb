// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

// Pops a string-to-string dictionary from the reader.
base::DictionaryValue* PopStringToStringDictionary(
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(NULL);
  if (!reader->PopArray(&array_reader))
    return NULL;
  scoped_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(NULL);
    std::string key;
    std::string value;
    if (!array_reader.PopDictEntry(&entry_reader) ||
        !entry_reader.PopString(&key) ||
        !entry_reader.PopString(&value))
      return NULL;
    result->SetWithoutPathExpansion(key,
                                    base::Value::CreateStringValue(value));
  }
  return result.release();
}

// Expects the reader to have a string-to-variant dictionary.
void ExpectDictionaryValueArgument(
    const base::DictionaryValue* expected_dictionary,
    dbus::MessageReader* reader) {
  dbus::MessageReader array_reader(NULL);
  ASSERT_TRUE(reader->PopArray(&array_reader));
  while (array_reader.HasMoreData()) {
    dbus::MessageReader entry_reader(NULL);
    ASSERT_TRUE(array_reader.PopDictEntry(&entry_reader));
    std::string key;
    ASSERT_TRUE(entry_reader.PopString(&key));
    dbus::MessageReader variant_reader(NULL);
    ASSERT_TRUE(entry_reader.PopVariant(&variant_reader));
    scoped_ptr<base::Value> value;
    // Variants in the dictionary can be basic types or string-to-string
    // dictinoary.
    switch (variant_reader.GetDataType()) {
      case dbus::Message::ARRAY:
        value.reset(PopStringToStringDictionary(&variant_reader));
        break;
      case dbus::Message::BOOL:
      case dbus::Message::INT32:
      case dbus::Message::STRING:
        value.reset(dbus::PopDataAsValue(&variant_reader));
        break;
      default:
        NOTREACHED();
    }
    ASSERT_TRUE(value.get());
    const base::Value* expected_value = NULL;
    EXPECT_TRUE(expected_dictionary->GetWithoutPathExpansion(key,
                                                             &expected_value));
    EXPECT_TRUE(value->Equals(expected_value));
  }
}

// Creates a DictionaryValue with example properties.
base::DictionaryValue* CreateExampleProperties() {
  base::DictionaryValue* properties = new base::DictionaryValue;
  properties->SetWithoutPathExpansion(
      flimflam::kGuidProperty,
      base::Value::CreateStringValue("00000000-0000-0000-0000-000000000000"));
  properties->SetWithoutPathExpansion(
      flimflam::kModeProperty,
      base::Value::CreateStringValue(flimflam::kModeManaged));
  properties->SetWithoutPathExpansion(
      flimflam::kTypeProperty,
      base::Value::CreateStringValue(flimflam::kTypeWifi));
  properties->SetWithoutPathExpansion(
      flimflam::kSSIDProperty,
      base::Value::CreateStringValue("testssid"));
  properties->SetWithoutPathExpansion(
      flimflam::kSecurityProperty,
      base::Value::CreateStringValue(flimflam::kSecurityPsk));
  return properties;
}

}  // namespace

class ShillManagerClientTest : public ShillClientUnittestBase {
 public:
  ShillManagerClientTest()
      : ShillClientUnittestBase(
          flimflam::kFlimflamManagerInterface,
          dbus::ObjectPath(flimflam::kFlimflamServicePath)) {
  }

  virtual void SetUp() {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillManagerClient::Create(REAL_DBUS_CLIENT_IMPLEMENTATION,
                                                mock_bus_));
    // Run the message loop to run the signal connection result callback.
    message_loop_.RunAllPending();
  }

  virtual void TearDown() {
    ShillClientUnittestBase::TearDown();
  }

 protected:
  scoped_ptr<ShillManagerClient> client_;
};

TEST_F(ShillManagerClientTest, PropertyChanged) {
  // Create a signal.
  base::FundamentalValue kOfflineMode(true);
  dbus::Signal signal(flimflam::kFlimflamManagerInterface,
                      flimflam::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(flimflam::kOfflineModeProperty);
  dbus::AppendBasicTypeValueData(&writer, kOfflineMode);

  // Set expectations.
  client_->SetPropertyChangedHandler(base::Bind(&ExpectPropertyChanged,
                                                flimflam::kOfflineModeProperty,
                                                &kOfflineMode));
  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Reset the handler.
  client_->ResetPropertyChangedHandler();
}

TEST_F(ShillManagerClientTest, GetProperties) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kOfflineModeProperty);
  entry_writer.AppendVariantOfBool(true);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kOfflineModeProperty,
                                base::Value::CreateBooleanValue(true));
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(base::Bind(&ExpectDictionaryValueResult,
                                    &value));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, CallGetPropertiesAndBlock) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(flimflam::kOfflineModeProperty);
  entry_writer.AppendVariantOfBool(true);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetWithoutPathExpansion(flimflam::kOfflineModeProperty,
                                base::Value::CreateBooleanValue(true));
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  scoped_ptr<base::DictionaryValue> result(
      client_->CallGetPropertiesAndBlock());
  EXPECT_TRUE(value.Equals(result.get()));
}

TEST_F(ShillManagerClientTest, SetProperty) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  base::StringValue value("portal list");
  PrepareForMethodCall(flimflam::kSetPropertyFunction,
                       base::Bind(ExpectStringAndValueArguments,
                                  flimflam::kCheckPortalListProperty,
                                  &value),
                       response.get());
  // Call method.
  client_->SetProperty(flimflam::kCheckPortalListProperty,
                       value,
                       base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, RequestScan) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(flimflam::kRequestScanFunction,
                       base::Bind(&ExpectStringArgument, flimflam::kTypeWifi),
                       response.get());
  // Call method.
  client_->RequestScan(flimflam::kTypeWifi, base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, EnableTechnology) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(flimflam::kEnableTechnologyFunction,
                       base::Bind(&ExpectStringArgument, flimflam::kTypeWifi),
                       response.get());
  // Call method.
  client_->EnableTechnology(flimflam::kTypeWifi,
                            base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, DisableTechnology) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(flimflam::kDisableTechnologyFunction,
                       base::Bind(&ExpectStringArgument, flimflam::kTypeWifi),
                       response.get());
  // Call method.
  client_->DisableTechnology(flimflam::kTypeWifi,
                             base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, ConfigureService) {
  // Create response.
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Create the argument dictionary.
  scoped_ptr<base::DictionaryValue> arg(CreateExampleProperties());
  // Set expectations.
  PrepareForMethodCall(flimflam::kConfigureServiceFunction,
                       base::Bind(&ExpectDictionaryValueArgument, arg.get()),
                       response.get());
  // Call method.
  client_->ConfigureService(*arg, base::Bind(&ExpectNoResultValue));
  // Run the message loop.
  message_loop_.RunAllPending();
}

TEST_F(ShillManagerClientTest, GetService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  scoped_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  scoped_ptr<base::DictionaryValue> arg(CreateExampleProperties());
  // Set expectations.
  PrepareForMethodCall(flimflam::kGetServiceFunction,
                       base::Bind(&ExpectDictionaryValueArgument, arg.get()),
                       response.get());
  // Call method.
  client_->GetService(*arg, base::Bind(&ExpectObjectPathResult, object_path));
  // Run the message loop.
  message_loop_.RunAllPending();
}

}  // namespace chromeos
