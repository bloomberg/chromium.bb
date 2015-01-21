// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_verify_delegate_chromeos.h"

#include "base/bind.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"

using chromeos::ShillManagerClient;

namespace {

ShillManagerClient* GetShillManagerClient() {
  return chromeos::DBusThreadManager::Get()->GetShillManagerClient();
}

bool GetServicePathFromGuid(const std::string& guid,
                            std::string* service_path,
                            std::string* error) {
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->GetNetworkStateFromGuid(guid);
  if (!network) {
    *error = extensions::networking_private::kErrorInvalidNetworkGuid;
    return false;
  }
  *service_path = network->path();
  return true;
}

ShillManagerClient::VerificationProperties ConvertVerificationProperties(
    const extensions::api::networking_private::VerificationProperties& input) {
  ShillManagerClient::VerificationProperties output;
  output.certificate = input.certificate;
  output.public_key = input.public_key;
  output.nonce = input.nonce;
  output.signed_data = input.signed_data;
  output.device_serial = input.device_serial;
  output.device_ssid = input.device_ssid;
  output.device_bssid = input.device_bssid;
  return output;
}

void ShillFailureCallback(
    const extensions::NetworkingPrivateDelegate::FailureCallback& callback,
    const std::string& error_name,
    const std::string& error_message) {
  callback.Run(error_name);
}

}  // namespace

namespace extensions {

NetworkingPrivateVerifyDelegateChromeOS::
    NetworkingPrivateVerifyDelegateChromeOS() {
}

NetworkingPrivateVerifyDelegateChromeOS::
    ~NetworkingPrivateVerifyDelegateChromeOS() {
}

void NetworkingPrivateVerifyDelegateChromeOS::VerifyDestination(
    const VerificationProperties& verification_properties,
    const BoolCallback& success_callback,
    const FailureCallback& failure_callback) {
  ShillManagerClient::VerificationProperties verification_property_struct =
      ConvertVerificationProperties(verification_properties);

  GetShillManagerClient()->VerifyDestination(
      verification_property_struct, success_callback,
      base::Bind(&ShillFailureCallback, failure_callback));
}

void NetworkingPrivateVerifyDelegateChromeOS::VerifyAndEncryptCredentials(
    const std::string& guid,
    const VerificationProperties& verification_properties,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  std::string service_path, error;
  if (!GetServicePathFromGuid(guid, &service_path, &error)) {
    failure_callback.Run(error);
    return;
  }

  ShillManagerClient::VerificationProperties verification_property_struct =
      ConvertVerificationProperties(verification_properties);

  GetShillManagerClient()->VerifyAndEncryptCredentials(
      verification_property_struct, service_path, success_callback,
      base::Bind(&ShillFailureCallback, failure_callback));
}

void NetworkingPrivateVerifyDelegateChromeOS::VerifyAndEncryptData(
    const VerificationProperties& verification_properties,
    const std::string& data,
    const StringCallback& success_callback,
    const FailureCallback& failure_callback) {
  ShillManagerClient::VerificationProperties verification_property_struct =
      ConvertVerificationProperties(verification_properties);

  GetShillManagerClient()->VerifyAndEncryptData(
      verification_property_struct, data, success_callback,
      base::Bind(&ShillFailureCallback, failure_callback));
}

}  // namespace extensions
