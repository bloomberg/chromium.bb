// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/managed_network_configuration_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_configuration_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

ManagedNetworkConfigurationHandler* g_configuration_handler_instance = NULL;

const char kLogModule[] = "ManagedNetworkConfigurationHandler";

// These are error strings used for error callbacks. None of these error
// messages are user-facing: they should only appear in logs.
const char kServicePath[] = "servicePath";
const char kSetOnUnconfiguredNetworkMessage[] =
    "Unable to modify properties of an unconfigured network.";
const char kSetOnUnconfiguredNetwork[] = "Error.SetCalledOnUnconfiguredNetwork";
const char kUnknownServicePathMessage[] = "Service path is unknown.";
const char kUnknownServicePath[] = "Error.UnknownServicePath";

void RunErrorCallback(const std::string& service_path,
                      const std::string& error_name,
                      const std::string& error_message,
                      const network_handler::ErrorCallback& error_callback) {
  network_event_log::AddEntry(kLogModule, error_name, error_message);
  error_callback.Run(
      error_name,
      make_scoped_ptr(
          network_handler::CreateErrorData(service_path,
                                           error_name,
                                           error_message)));
}

void TranslatePropertiesAndRunCallback(
    const network_handler::DictionaryResultCallback& callback,
    const std::string& service_path,
    const base::DictionaryValue& shill_properties) {
  scoped_ptr<base::DictionaryValue> onc_network(
      onc::TranslateShillServiceToONCPart(
          shill_properties,
          &onc::kNetworkConfigurationSignature));
  callback.Run(service_path, *onc_network);
}

}  // namespace

// static
void ManagedNetworkConfigurationHandler::Initialize() {
  CHECK(!g_configuration_handler_instance);
  g_configuration_handler_instance = new ManagedNetworkConfigurationHandler;
}

// static
bool ManagedNetworkConfigurationHandler::IsInitialized() {
  return g_configuration_handler_instance;
}

// static
void ManagedNetworkConfigurationHandler::Shutdown() {
  CHECK(g_configuration_handler_instance);
  delete g_configuration_handler_instance;
  g_configuration_handler_instance = NULL;
}

// static
ManagedNetworkConfigurationHandler* ManagedNetworkConfigurationHandler::Get() {
  CHECK(g_configuration_handler_instance);
  return g_configuration_handler_instance;
}

void ManagedNetworkConfigurationHandler::GetProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  // TODO(pneubeck): Merge with policies.
  NetworkConfigurationHandler::Get()->GetProperties(
      service_path,
      base::Bind(&TranslatePropertiesAndRunCallback, callback),
      error_callback);
}

void ManagedNetworkConfigurationHandler::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  const NetworkState* state =
      NetworkStateHandler::Get()->GetNetworkState(service_path);

  if (!state) {
    RunErrorCallback(service_path,
                     kUnknownServicePath,
                     kUnknownServicePathMessage,
                     error_callback);
  }
  std::string guid = state->guid();
  if (guid.empty()) {
    RunErrorCallback(service_path,
                     kSetOnUnconfiguredNetwork,
                     kSetOnUnconfiguredNetworkMessage,
                     error_callback);
  }

  // TODO(pneubeck): Enforce policies.

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(
          &onc::kNetworkConfigurationSignature,
          properties));

  NetworkConfigurationHandler::Get()->SetProperties(service_path,
                                                    *shill_dictionary,
                                                    callback,
                                                    error_callback);
}

void ManagedNetworkConfigurationHandler::Connect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  // TODO(pneubeck): Update the user profile with tracked/followed settings of
  // the shared profile.
  NetworkConfigurationHandler::Get()->Connect(service_path,
                                              callback,
                                              error_callback);
}

void ManagedNetworkConfigurationHandler::Disconnect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->Disconnect(service_path,
                                                 callback,
                                                 error_callback);
}

void ManagedNetworkConfigurationHandler::CreateConfiguration(
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  scoped_ptr<base::DictionaryValue> modified_properties(
      properties.DeepCopy());

  // If there isn't already a GUID attached to these properties, then
  // generate one and add it.
  std::string guid;
  if (!properties.GetString(onc::network_config::kGUID, &guid)) {
    guid = base::GenerateGUID();
    modified_properties->SetStringWithoutPathExpansion(
        onc::network_config::kGUID, guid);
  } else {
    NOTREACHED(); // TODO(pneubeck): Return an error using error_callback.
  }

  // TODO(pneubeck): Enforce policies.

  scoped_ptr<base::DictionaryValue> shill_dictionary(
      onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                     properties));

  NetworkConfigurationHandler::Get()->CreateConfiguration(*shill_dictionary,
                                                          callback,
                                                          error_callback);
}

void ManagedNetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  NetworkConfigurationHandler::Get()->RemoveConfiguration(service_path,
                                                          callback,
                                                          error_callback);
}

ManagedNetworkConfigurationHandler::ManagedNetworkConfigurationHandler() {
}

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() {
}

}  // namespace chromeos
