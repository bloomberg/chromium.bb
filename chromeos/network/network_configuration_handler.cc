// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_configuration_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chromeos/dbus/dbus_method_call_status.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "chromeos/dbus/shill_service_client.h"
#include "dbus/object_path.h"

namespace chromeos {

namespace {

const char kLogModule[] = "NetworkConfigurationHandler";

NetworkConfigurationHandler* g_configuration_handler_instance = NULL;

// None of these error messages are user-facing: they should only appear in
// logs.
const char kErrorsListTag[] = "errors";
const char kClearPropertiesFailedError[] = "Error.ClearPropertiesFailed";
const char kClearPropertiesFailedErrorMessage[] = "Clear properties failed";
const char kDBusFailedError[] = "Error.DBusFailed";
const char kDBusFailedErrorMessage[] = "DBus call failed.";

void ClearPropertiesCallback(
    const std::vector<std::string>& names,
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback,
    const base::ListValue& result) {
  bool some_failed = false;
  for (size_t i = 0; i < result.GetSize(); ++i) {
    bool success;
    if (result.GetBoolean(i, &success)) {
      if (!success) {
        some_failed = true;
        break;
      }
    } else {
      NOTREACHED() << "Result garbled from ClearProperties";
    }
  }

  if (some_failed) {
    DCHECK(names.size() == result.GetSize())
        << "Result wrong size from ClearProperties.";
    scoped_ptr<base::DictionaryValue> error_data(
        network_handler::CreateErrorData(service_path,
                                         kClearPropertiesFailedError,
                                         kClearPropertiesFailedErrorMessage));
    LOG(ERROR) << "ClearPropertiesCallback failed for service path: "
               << service_path;
    error_data->Set("errors", result.DeepCopy());
    scoped_ptr<base::ListValue> name_list(new base::ListValue);
    name_list->AppendStrings(names);
    error_data->Set("names", name_list.release());
    error_callback.Run(kClearPropertiesFailedError, error_data.Pass());
  } else {
    callback.Run();
  }
}

// Used to translate the dbus dictionary callback into one that calls
// the error callback if we have a failure.
void RunCallbackWithDictionaryValue(
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback,
    const std::string& service_path,
    DBusMethodCallStatus call_status,
    const base::DictionaryValue& value) {
  if (call_status != DBUS_METHOD_CALL_SUCCESS) {
    scoped_ptr<base::DictionaryValue> error_data(
        network_handler::CreateErrorData(service_path,
                                         kDBusFailedError,
                                         kDBusFailedErrorMessage));
    LOG(ERROR) << "CallbackWithDictionaryValue failed for service path: "
               << service_path;
    error_callback.Run(kDBusFailedError, error_data.Pass());
  } else {
    callback.Run(service_path, value);
  }
}

void RunCreateNetworkCallback(
    const network_handler::StringResultCallback& callback,
    const dbus::ObjectPath& service_path) {
  callback.Run(service_path.value());
}

void IgnoreObjectPathCallback(const base::Closure& callback,
                              const dbus::ObjectPath& object_path) {
  callback.Run();
}

}  // namespace

// static
void NetworkConfigurationHandler::Initialize() {
  CHECK(!g_configuration_handler_instance);
  g_configuration_handler_instance = new NetworkConfigurationHandler;
}

// static
void NetworkConfigurationHandler::Shutdown() {
  CHECK(g_configuration_handler_instance);
  delete g_configuration_handler_instance;
  g_configuration_handler_instance = NULL;
}

// static
NetworkConfigurationHandler* NetworkConfigurationHandler::Get() {
  CHECK(g_configuration_handler_instance)
      << "NetworkConfigurationHandler::Get() called before Initialize()";
  return g_configuration_handler_instance;
}

void NetworkConfigurationHandler::GetProperties(
    const std::string& service_path,
    const network_handler::DictionaryResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillServiceClient()->GetProperties(
      dbus::ObjectPath(service_path),
      base::Bind(&RunCallbackWithDictionaryValue,
                 callback,
                 error_callback,
                 service_path));
}

void NetworkConfigurationHandler::SetProperties(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillManagerClient()->ConfigureService(
      properties,
      base::Bind(&IgnoreObjectPathCallback, callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, service_path, error_callback));
}

void NetworkConfigurationHandler::ClearProperties(
    const std::string& service_path,
    const std::vector<std::string>& names,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) {
  DBusThreadManager::Get()->GetShillServiceClient()->ClearProperties(
      dbus::ObjectPath(service_path),
      names,
      base::Bind(&ClearPropertiesCallback,
                 names,
                 service_path,
                 callback,
                 error_callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, service_path, error_callback));
}

void NetworkConfigurationHandler::Connect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath(service_path),
      callback,
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, service_path, error_callback));
}

void NetworkConfigurationHandler::Disconnect(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillServiceClient()->Disconnect(
      dbus::ObjectPath(service_path),
      callback,
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, service_path, error_callback));
}

void NetworkConfigurationHandler::CreateConfiguration(
    const base::DictionaryValue& properties,
    const network_handler::StringResultCallback& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillManagerClient()->GetService(
      properties,
      base::Bind(&RunCreateNetworkCallback, callback),
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, "", error_callback));
}

void NetworkConfigurationHandler::RemoveConfiguration(
    const std::string& service_path,
    const base::Closure& callback,
    const network_handler::ErrorCallback& error_callback) const {
  DBusThreadManager::Get()->GetShillServiceClient()->Remove(
      dbus::ObjectPath(service_path),
      callback,
      base::Bind(&network_handler::ShillErrorCallbackFunction,
                 kLogModule, service_path, error_callback));
}

NetworkConfigurationHandler::NetworkConfigurationHandler() {
}

NetworkConfigurationHandler::~NetworkConfigurationHandler() {
}

}  // namespace chromeos
