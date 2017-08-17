// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/shill_manager_client.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "chromeos/dbus/shill_client_unittest_base.h"
#include "dbus/message.h"
#include "dbus/object_path.h"
#include "dbus/values_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using testing::_;
using testing::ByRef;

namespace chromeos {

namespace {

void ExpectStringArguments(const std::vector<std::string>& arguments,
                           dbus::MessageReader* reader) {
  for (std::vector<std::string>::const_iterator iter = arguments.begin();
       iter != arguments.end(); ++iter) {
    std::string arg_string;
    ASSERT_TRUE(reader->PopString(&arg_string));
    EXPECT_EQ(*iter, arg_string);
  }
  EXPECT_FALSE(reader->HasMoreData());
}

void ExpectStringArgumentsFollowedByObjectPath(
      const std::vector<std::string>& arguments,
      const dbus::ObjectPath& object_path,
      dbus::MessageReader* reader) {
  for (std::vector<std::string>::const_iterator iter = arguments.begin();
       iter != arguments.end(); ++iter) {
    std::string arg_string;
    ASSERT_TRUE(reader->PopString(&arg_string));
    EXPECT_EQ(*iter, arg_string);
  }
  dbus::ObjectPath path;
  ASSERT_TRUE(reader->PopObjectPath(&path));
  EXPECT_EQ(object_path, path);
  EXPECT_FALSE(reader->HasMoreData());
}

void ExpectThrottlingArguments(bool throttling_enabled_expected,
                               uint32_t upload_rate_kbits_expected,
                               uint32_t download_rate_kbits_expected,
                               dbus::MessageReader* reader) {
  bool throttling_enabled_actual;
  uint32_t upload_rate_kbits_actual;
  uint32_t download_rate_kbits_actual;
  ASSERT_TRUE(reader->PopBool(&throttling_enabled_actual));
  EXPECT_EQ(throttling_enabled_actual, throttling_enabled_expected);
  ASSERT_TRUE(reader->PopUint32(&upload_rate_kbits_actual));
  EXPECT_EQ(upload_rate_kbits_expected, upload_rate_kbits_actual);
  ASSERT_TRUE(reader->PopUint32(&download_rate_kbits_actual));
  EXPECT_EQ(download_rate_kbits_expected, download_rate_kbits_actual);
  EXPECT_FALSE(reader->HasMoreData());
}

}  // namespace

class ShillManagerClientTest : public ShillClientUnittestBase {
 public:
  ShillManagerClientTest()
      : ShillClientUnittestBase(shill::kFlimflamManagerInterface,
                                dbus::ObjectPath(shill::kFlimflamServicePath)) {
  }

  void SetUp() override {
    ShillClientUnittestBase::SetUp();
    // Create a client with the mock bus.
    client_.reset(ShillManagerClient::Create());
    client_->Init(mock_bus_.get());
    // Run the message loop to run the signal connection result callback.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override { ShillClientUnittestBase::TearDown(); }

 protected:
  std::unique_ptr<ShillManagerClient> client_;
};

TEST_F(ShillManagerClientTest, PropertyChanged) {
  // Create a signal.
  base::Value kOfflineMode(true);
  dbus::Signal signal(shill::kFlimflamManagerInterface,
                      shill::kMonitorPropertyChanged);
  dbus::MessageWriter writer(&signal);
  writer.AppendString(shill::kOfflineModeProperty);
  dbus::AppendBasicTypeValueData(&writer, kOfflineMode);

  // Set expectations.
  MockPropertyChangeObserver observer;
  EXPECT_CALL(observer,
              OnPropertyChanged(shill::kOfflineModeProperty,
                                ValueEq(ByRef(kOfflineMode)))).Times(1);

  // Add the observer
  client_->AddPropertyChangedObserver(&observer);

  // Run the signal callback.
  SendPropertyChangedSignal(&signal);

  // Remove the observer.
  client_->RemovePropertyChangedObserver(&observer);

  // Make sure it's not called anymore.
  EXPECT_CALL(observer, OnPropertyChanged(_, _)).Times(0);

  // Run the signal callback again and make sure the observer isn't called.
  SendPropertyChangedSignal(&signal);
}

TEST_F(ShillManagerClientTest, GetProperties) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter array_writer(NULL);
  writer.OpenArray("{sv}", &array_writer);
  dbus::MessageWriter entry_writer(NULL);
  array_writer.OpenDictEntry(&entry_writer);
  entry_writer.AppendString(shill::kOfflineModeProperty);
  entry_writer.AppendVariantOfBool(true);
  array_writer.CloseContainer(&entry_writer);
  writer.CloseContainer(&array_writer);

  // Create the expected value.
  base::DictionaryValue value;
  value.SetKey(shill::kOfflineModeProperty, base::Value(true));
  // Set expectations.
  PrepareForMethodCall(shill::kGetPropertiesFunction,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetProperties(base::Bind(&ExpectDictionaryValueResult, &value));
  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, GetNetworksForGeolocation) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());

  dbus::MessageWriter writer(response.get());
  dbus::MessageWriter type_dict_writer(NULL);
  writer.OpenArray("{sv}", &type_dict_writer);
  dbus::MessageWriter type_entry_writer(NULL);
  type_dict_writer.OpenDictEntry(&type_entry_writer);
  type_entry_writer.AppendString(shill::kTypeWifi);
  dbus::MessageWriter variant_writer(NULL);
  type_entry_writer.OpenVariant("aa{ss}", &variant_writer);
  dbus::MessageWriter wap_list_writer(NULL);
  variant_writer.OpenArray("a{ss}", &wap_list_writer);
  dbus::MessageWriter property_dict_writer(NULL);
  wap_list_writer.OpenArray("{ss}", &property_dict_writer);
  dbus::MessageWriter property_entry_writer(NULL);
  property_dict_writer.OpenDictEntry(&property_entry_writer);
  property_entry_writer.AppendString(shill::kGeoMacAddressProperty);
  property_entry_writer.AppendString("01:23:45:67:89:AB");
  property_dict_writer.CloseContainer(&property_entry_writer);
  wap_list_writer.CloseContainer(&property_dict_writer);
  variant_writer.CloseContainer(&wap_list_writer);
  type_entry_writer.CloseContainer(&wap_list_writer);
  type_dict_writer.CloseContainer(&type_entry_writer);
  writer.CloseContainer(&type_dict_writer);


  // Create the expected value.
  auto property_dict_value = base::MakeUnique<base::DictionaryValue>();
  property_dict_value->SetKey(shill::kGeoMacAddressProperty,
                              base::Value("01:23:45:67:89:AB"));
  auto type_entry_value = base::MakeUnique<base::ListValue>();
  type_entry_value->Append(std::move(property_dict_value));
  base::DictionaryValue type_dict_value;
  type_dict_value.SetWithoutPathExpansion("wifi", std::move(type_entry_value));

  // Set expectations.
  PrepareForMethodCall(shill::kGetNetworksForGeolocation,
                       base::Bind(&ExpectNoArgument),
                       response.get());
  // Call method.
  client_->GetNetworksForGeolocation(base::Bind(&ExpectDictionaryValueResult,
                                                &type_dict_value));

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, SetProperty) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  base::Value value("portal list");
  PrepareForMethodCall(shill::kSetPropertyFunction,
                       base::Bind(ExpectStringAndValueArguments,
                                  shill::kCheckPortalListProperty,
                                  &value),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->SetProperty(shill::kCheckPortalListProperty, value,
                       mock_closure.Get(), mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, RequestScan) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(shill::kRequestScanFunction,
                       base::Bind(&ExpectStringArgument, shill::kTypeWifi),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->RequestScan(shill::kTypeWifi, mock_closure.Get(),
                       mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, EnableTechnology) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(shill::kEnableTechnologyFunction,
                       base::Bind(&ExpectStringArgument, shill::kTypeWifi),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->EnableTechnology(shill::kTypeWifi, mock_closure.Get(),
                            mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, NetworkThrottling) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  const bool enabled = true;
  const uint32_t upload_rate = 1200;
  const uint32_t download_rate = 2000;
  PrepareForMethodCall(shill::kSetNetworkThrottlingFunction,
                       base::Bind(&ExpectThrottlingArguments, enabled,
                                  upload_rate, download_rate),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->SetNetworkThrottlingStatus(enabled, upload_rate, download_rate,
                                      mock_closure.Get(),
                                      mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, DisableTechnology) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  // Set expectations.
  PrepareForMethodCall(shill::kDisableTechnologyFunction,
                       base::Bind(&ExpectStringArgument, shill::kTypeWifi),
                       response.get());
  // Call method.
  base::MockCallback<base::Closure> mock_closure;
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->DisableTechnology(shill::kTypeWifi, mock_closure.Get(),
                             mock_error_callback.Get());
  EXPECT_CALL(mock_closure, Run()).Times(1);
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, ConfigureService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  std::unique_ptr<base::DictionaryValue> arg(CreateExampleServiceProperties());
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kConfigureServiceFunction,
      base::Bind(&ExpectDictionaryValueArgument, arg.get(), string_valued),
      response.get());
  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->ConfigureService(
      *arg, base::Bind(&ExpectObjectPathResultWithoutStatus, object_path),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, GetService) {
  // Create response.
  const dbus::ObjectPath object_path("/");
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  writer.AppendObjectPath(object_path);
  // Create the argument dictionary.
  std::unique_ptr<base::DictionaryValue> arg(CreateExampleServiceProperties());
  // Use a variant valued dictionary rather than a string valued one.
  const bool string_valued = false;
  // Set expectations.
  PrepareForMethodCall(
      shill::kGetServiceFunction,
      base::Bind(&ExpectDictionaryValueArgument, arg.get(), string_valued),
      response.get());
  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  client_->GetService(
      *arg, base::Bind(&ExpectObjectPathResultWithoutStatus, object_path),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, VerifyDestination) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  bool expected = true;
  writer.AppendBool(expected);
  // Set expectations.
  std::vector<std::string> arguments;
  arguments.push_back("certificate");
  arguments.push_back("public_key");
  arguments.push_back("nonce");
  arguments.push_back("signed_data");
  arguments.push_back("device_serial");
  arguments.push_back("device_ssid");
  arguments.push_back("device_bssid");
  PrepareForMethodCall(shill::kVerifyDestinationFunction,
                       base::Bind(&ExpectStringArguments, arguments),
                       response.get());

  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  ShillManagerClient::VerificationProperties properties;
  properties.certificate = arguments[0];
  properties.public_key = arguments[1];
  properties.nonce = arguments[2];
  properties.signed_data = arguments[3];
  properties.device_serial = arguments[4];
  properties.device_ssid = arguments[5];
  properties.device_bssid = arguments[6];
  client_->VerifyDestination(
      properties, base::Bind(&ExpectBoolResultWithoutStatus, expected),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, VerifyAndEncryptCredentials) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  std::string expected = "encrypted_credentials";
  writer.AppendString(expected);
  // Set expectations.
  std::vector<std::string> arguments;
  arguments.push_back("certificate");
  arguments.push_back("public_key");
  arguments.push_back("nonce");
  arguments.push_back("signed_data");
  arguments.push_back("device_serial");
  arguments.push_back("device_ssid");
  arguments.push_back("device_bssid");
  std::string service_path = "/";
  dbus::ObjectPath service_path_obj(service_path);
  PrepareForMethodCall(shill::kVerifyAndEncryptCredentialsFunction,
                       base::Bind(&ExpectStringArgumentsFollowedByObjectPath,
                                  arguments,
                                  service_path_obj),
                       response.get());

  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  ShillManagerClient::VerificationProperties properties;
  properties.certificate = arguments[0];
  properties.public_key = arguments[1];
  properties.nonce = arguments[2];
  properties.signed_data = arguments[3];
  properties.device_serial = arguments[4];
  properties.device_ssid = arguments[5];
  properties.device_bssid = arguments[6];
  client_->VerifyAndEncryptCredentials(
      properties, service_path,
      base::Bind(&ExpectStringResultWithoutStatus, expected),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

TEST_F(ShillManagerClientTest, VerifyAndEncryptData) {
  // Create response.
  std::unique_ptr<dbus::Response> response(dbus::Response::CreateEmpty());
  dbus::MessageWriter writer(response.get());
  std::string expected = "encrypted_data";
  writer.AppendString(expected);
  // Set expectations.
  std::vector<std::string> arguments;
  arguments.push_back("certificate");
  arguments.push_back("public_key");
  arguments.push_back("nonce");
  arguments.push_back("signed_data");
  arguments.push_back("device_serial");
  arguments.push_back("device_ssid");
  arguments.push_back("device_bssid");
  arguments.push_back("data");
  PrepareForMethodCall(shill::kVerifyAndEncryptDataFunction,
                       base::Bind(&ExpectStringArguments, arguments),
                       response.get());

  // Call method.
  base::MockCallback<ShillManagerClient::ErrorCallback> mock_error_callback;
  ShillManagerClient::VerificationProperties properties;
  properties.certificate = arguments[0];
  properties.public_key = arguments[1];
  properties.nonce = arguments[2];
  properties.signed_data = arguments[3];
  properties.device_serial = arguments[4];
  properties.device_ssid = arguments[5];
  properties.device_bssid = arguments[6];
  client_->VerifyAndEncryptData(
      properties, arguments[7],
      base::Bind(&ExpectStringResultWithoutStatus, expected),
      mock_error_callback.Get());
  EXPECT_CALL(mock_error_callback, Run(_, _)).Times(0);

  // Run the message loop.
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
