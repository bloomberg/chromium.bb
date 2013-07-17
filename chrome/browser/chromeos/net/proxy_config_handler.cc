// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/proxy_config_handler.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace proxy_config {

scoped_ptr<ProxyConfigDictionary> GetProxyConfigForNetwork(
    const NetworkState& network) {
  const base::DictionaryValue& value = network.proxy_config();
  if (value.empty())
    return scoped_ptr<ProxyConfigDictionary>();
  return make_scoped_ptr(new ProxyConfigDictionary(&value));
}

void SetProxyConfigForNetwork(const ProxyConfigDictionary& proxy_config,
                              const NetworkState& network) {
  chromeos::ShillServiceClient* shill_service_client =
      DBusThreadManager::Get()->GetShillServiceClient();

  ProxyPrefs::ProxyMode mode;
  if (!proxy_config.GetMode(&mode) || mode == ProxyPrefs::MODE_DIRECT) {
    // TODO(pneubeck): Consider removing this legacy code.  Return empty string
    // for direct mode for portal check to work correctly.
    shill_service_client->ClearProperty(
        dbus::ObjectPath(network.path()),
        flimflam::kProxyConfigProperty,
        base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "SetProxyConfig.ClearProperty Failed",
                   network.path(), network_handler::ErrorCallback()));
  } else {
    std::string proxy_config_str;
    base::JSONWriter::Write(&proxy_config.GetDictionary(), &proxy_config_str);
    shill_service_client->SetProperty(
        dbus::ObjectPath(network.path()),
        flimflam::kProxyConfigProperty,
        base::StringValue(proxy_config_str),
        base::Bind(&base::DoNothing),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "SetProxyConfig.SetProperty Failed",
                   network.path(), network_handler::ErrorCallback()));
  }

  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->
        RequestUpdateForNetwork(network.path());
  }
}

void RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUseSharedProxies,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace proxy_config

}  // namespace chromeos
