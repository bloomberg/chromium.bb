// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/proxy_config_handler.h"

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/shill_service_client.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const base::DictionaryValue* GetNetworkConfigByGUID(
    const base::ListValue& network_configs,
    const std::string& guid) {
  for (base::ListValue::const_iterator it = network_configs.begin();
       it != network_configs.end();
       ++it) {
    const base::DictionaryValue* network = NULL;
    (*it)->GetAsDictionary(&network);
    std::string current_guid;
    network->GetStringWithoutPathExpansion(onc::network_config::kGUID,
                                           &current_guid);
    if (current_guid == guid)
      return network;
  }
  return NULL;
}

scoped_ptr<ProxyConfigDictionary> GetProxyPolicy(
    const PrefService* pref_service,
    const char* pref_name,
    const NetworkState& network,
    bool* network_is_managed) {
  *network_is_managed = false;

  if (!pref_service || network.guid().empty())
    return scoped_ptr<ProxyConfigDictionary>();

  if (!pref_service->IsManagedPreference(pref_name)) {
    // No policy set.
    return scoped_ptr<ProxyConfigDictionary>();
  }

  const base::ListValue* onc_policy = pref_service->GetList(pref_name);
  if (!onc_policy) {
    LOG(ERROR) << "Pref " << pref_name << " is managed, but no value is set.";
    return scoped_ptr<ProxyConfigDictionary>();
  }

  const base::DictionaryValue* network_policy =
      GetNetworkConfigByGUID(*onc_policy, network.guid());
  if (!network_policy) {
    // This network isn't managed by this policy.
    return scoped_ptr<ProxyConfigDictionary>();
  }

  const base::DictionaryValue* proxy_policy = NULL;
  network_policy->GetDictionaryWithoutPathExpansion(
      onc::network_config::kProxySettings, &proxy_policy);
  if (!proxy_policy) {
    // This policy doesn't set a proxy for this network. Nonetheless, this
    // disallows changes by the user.
    *network_is_managed = true;
    return scoped_ptr<ProxyConfigDictionary>();
  }

  scoped_ptr<base::DictionaryValue> proxy_dict =
      onc::ConvertOncProxySettingsToProxyConfig(*proxy_policy);
  *network_is_managed = true;
  return make_scoped_ptr(new ProxyConfigDictionary(proxy_dict.get()));
}

void NotifyNetworkStateHandler(const std::string& service_path) {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RequestUpdateForNetwork(
        service_path);
  }
}

}  // namespace

namespace proxy_config {

scoped_ptr<ProxyConfigDictionary> GetProxyConfigForNetwork(
    const PrefService* profile_prefs,
    const PrefService* local_state_prefs,
    const NetworkState& network,
    onc::ONCSource* onc_source) {
  VLOG(2) << "GetProxyConfigForNetwork network: " << network.path()
          << " , guid: " << network.guid();
  *onc_source = onc::ONC_SOURCE_NONE;
  bool network_is_managed = false;

  scoped_ptr<ProxyConfigDictionary> proxy_config =
      GetProxyPolicy(profile_prefs,
                     prefs::kOpenNetworkConfiguration,
                     network,
                     &network_is_managed);
  if (network_is_managed) {
    VLOG(1) << "Network " << network.path() << " is managed by user policy.";
    *onc_source = onc::ONC_SOURCE_USER_POLICY;
    return proxy_config.Pass();
  }
  proxy_config = GetProxyPolicy(local_state_prefs,
                                prefs::kDeviceOpenNetworkConfiguration,
                                network,
                                &network_is_managed);
  if (network_is_managed) {
    VLOG(1) << "Network " << network.path() << " is managed by device policy.";
    *onc_source = onc::ONC_SOURCE_DEVICE_POLICY;
    return proxy_config.Pass();
  }

  if (network.profile_path().empty())
    return scoped_ptr<ProxyConfigDictionary>();

  const NetworkProfile* profile = NetworkHandler::Get()
      ->network_profile_handler()->GetProfileForPath(network.profile_path());
  if (!profile) {
    LOG(WARNING) << "Unknown profile_path '" << network.profile_path() << "'.";
    return scoped_ptr<ProxyConfigDictionary>();
  }
  if (!profile_prefs && profile->type() == NetworkProfile::TYPE_USER) {
    // This case occurs, for example, if called from the proxy config tracker
    // created for the system request context and the signin screen. Both don't
    // use profile prefs and shouldn't depend on the user's not shared proxy
    // settings.
    VLOG(1)
        << "Don't use unshared settings for system context or signin screen.";
    return scoped_ptr<ProxyConfigDictionary>();
  }

  // No policy set for this network, read instead the user's (shared or
  // unshared) configuration.
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
    // Return empty string for direct mode for portal check to work correctly.
    // TODO(pneubeck): Consider removing this legacy code.
    shill_service_client->ClearProperty(
        dbus::ObjectPath(network.path()),
        flimflam::kProxyConfigProperty,
        base::Bind(&NotifyNetworkStateHandler, network.path()),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "SetProxyConfig.ClearProperty Failed",
                   network.path(),
                   network_handler::ErrorCallback()));
  } else {
    std::string proxy_config_str;
    base::JSONWriter::Write(&proxy_config.GetDictionary(), &proxy_config_str);
    shill_service_client->SetProperty(
        dbus::ObjectPath(network.path()),
        flimflam::kProxyConfigProperty,
        base::StringValue(proxy_config_str),
        base::Bind(&NotifyNetworkStateHandler, network.path()),
        base::Bind(&network_handler::ShillErrorCallbackFunction,
                   "SetProxyConfig.SetProperty Failed",
                   network.path(),
                   network_handler::ErrorCallback()));
  }
}

void RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kDeviceOpenNetworkConfiguration);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(
      prefs::kUseSharedProxies,
      false,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);

  registry->RegisterListPref(prefs::kOpenNetworkConfiguration,
                             user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

}  // namespace proxy_config

}  // namespace chromeos
