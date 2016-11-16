// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/proxy/ui_proxy_config_service.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/proxy/proxy_config_handler.h"
#include "chromeos/network/proxy/proxy_config_service_impl.h"
#include "components/device_event_log/device_event_log.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "net/proxy/proxy_config.h"

namespace chromeos {

namespace {

const char* ModeToString(UIProxyConfig::Mode mode) {
  switch (mode) {
    case UIProxyConfig::MODE_DIRECT:
      return "direct";
    case UIProxyConfig::MODE_AUTO_DETECT:
      return "auto-detect";
    case UIProxyConfig::MODE_PAC_SCRIPT:
      return "pacurl";
    case UIProxyConfig::MODE_SINGLE_PROXY:
      return "single-proxy";
    case UIProxyConfig::MODE_PROXY_PER_SCHEME:
      return "proxy-per-scheme";
  }
  NOTREACHED() << "Unrecognized mode type";
  return "";
}

// Writes the proxy config of |network| to |proxy_config|.  Sets |onc_source| to
// the source of this configuration. Returns false if no proxy was configured
// for this network.
bool GetProxyConfig(const PrefService* profile_prefs,
                    const PrefService* local_state_prefs,
                    const NetworkState& network,
                    net::ProxyConfig* proxy_config,
                    onc::ONCSource* onc_source) {
  std::unique_ptr<ProxyConfigDictionary> proxy_dict =
      proxy_config::GetProxyConfigForNetwork(profile_prefs, local_state_prefs,
                                             network, onc_source);
  if (!proxy_dict)
    return false;
  return PrefProxyConfigTrackerImpl::PrefConfigToNetConfig(*proxy_dict,
                                                           proxy_config);
}

// Returns true if proxy settings from |onc_source| are editable.
bool IsNetworkProxySettingsEditable(const onc::ONCSource onc_source) {
  return onc_source != onc::ONC_SOURCE_DEVICE_POLICY &&
         onc_source != onc::ONC_SOURCE_USER_POLICY;
}

}  // namespace

UIProxyConfigService::UIProxyConfigService(PrefService* profile_prefs,
                                           PrefService* local_state_prefs)
    : profile_prefs_(profile_prefs), local_state_prefs_(local_state_prefs) {
  if (profile_prefs_) {
    profile_registrar_.Init(profile_prefs_);
    profile_registrar_.Add(
        ::proxy_config::prefs::kProxy,
        base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                   base::Unretained(this)));
    profile_registrar_.Add(
        ::proxy_config::prefs::kUseSharedProxies,
        base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                   base::Unretained(this)));
  }

  DCHECK(local_state_prefs_);
  local_state_registrar_.Init(local_state_prefs_);
  local_state_registrar_.Add(
      ::proxy_config::prefs::kProxy,
      base::Bind(&UIProxyConfigService::OnPreferenceChanged,
                 base::Unretained(this)));
}

UIProxyConfigService::~UIProxyConfigService() {}

void UIProxyConfigService::UpdateFromPrefs(const std::string& network_guid) {
  current_ui_network_guid_ = network_guid;
  const NetworkState* network = nullptr;
  if (!network_guid.empty()) {
    network =
        NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
            network_guid);
    if (!network) {
      NET_LOG(ERROR) << "No NetworkState for guid: " << network_guid;
    } else if (!network->IsInProfile()) {
      NET_LOG(ERROR) << "Network not in profile: " << network_guid;
      network = nullptr;
    }
  }
  if (!network) {
    current_ui_network_guid_.clear();
    current_ui_config_ = UIProxyConfig();
    return;
  }

  DetermineEffectiveConfig(*network);
  VLOG(1) << "Current ui network: " << network->name() << ", "
          << ModeToString(current_ui_config_.mode) << ", "
          << ProxyPrefs::ConfigStateToDebugString(current_ui_config_.state)
          << ", modifiable:" << current_ui_config_.user_modifiable;
}

void UIProxyConfigService::GetProxyConfig(const std::string& network_guid,
                                          UIProxyConfig* config) {
  if (network_guid != current_ui_network_guid_)
    UpdateFromPrefs(network_guid);
  *config = current_ui_config_;
}

void UIProxyConfigService::SetProxyConfig(const std::string& network_guid,
                                          const UIProxyConfig& config) {
  current_ui_network_guid_ = network_guid;
  current_ui_config_ = config;
  if (current_ui_network_guid_.empty())
    return;

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          current_ui_network_guid_);
  if (!network || !network->IsInProfile()) {
    NET_LOG(ERROR) << "No configured NetworkState for guid: "
                   << current_ui_network_guid_;
    return;
  }

  // Store config for this network.
  std::unique_ptr<base::DictionaryValue> proxy_config_value(
      config.ToPrefProxyConfig());
  ProxyConfigDictionary proxy_config_dict(proxy_config_value.get());

  VLOG(1) << "Set proxy for " << current_ui_network_guid_ << " to "
          << *proxy_config_value;

  proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
  current_ui_config_.state = ProxyPrefs::CONFIG_SYSTEM;
}

void UIProxyConfigService::DetermineEffectiveConfig(
    const NetworkState& network) {
  DCHECK(local_state_prefs_);

  // The pref service to read proxy settings that apply to all networks.
  // Settings from the profile overrule local state.
  PrefService* top_pref_service =
      profile_prefs_ ? profile_prefs_ : local_state_prefs_;

  // Get prefs proxy config if available.
  net::ProxyConfig pref_config;
  ProxyPrefs::ConfigState pref_state =
      ProxyConfigServiceImpl::ReadPrefConfig(top_pref_service, &pref_config);

  // Get network proxy config if available.
  net::ProxyConfig network_config;
  net::ProxyConfigService::ConfigAvailability network_availability =
      net::ProxyConfigService::CONFIG_UNSET;
  onc::ONCSource onc_source = onc::ONC_SOURCE_NONE;
  if (chromeos::GetProxyConfig(profile_prefs_, local_state_prefs_, network,
                               &network_config, &onc_source)) {
    // Network is private or shared with user using shared proxies.
    VLOG(1) << this << ": using proxy of network: " << network.path();
    network_availability = net::ProxyConfigService::CONFIG_VALID;
  }

  // Determine effective proxy config, either from prefs or network.
  ProxyPrefs::ConfigState effective_config_state;
  net::ProxyConfig effective_config;
  ProxyConfigServiceImpl::GetEffectiveProxyConfig(
      pref_state, pref_config, network_availability, network_config, false,
      &effective_config_state, &effective_config);

  // Store effective proxy into |current_ui_config_|.
  current_ui_config_.FromNetProxyConfig(effective_config);
  current_ui_config_.state = effective_config_state;
  if (ProxyConfigServiceImpl::PrefPrecedes(effective_config_state)) {
    current_ui_config_.user_modifiable = false;
  } else if (!IsNetworkProxySettingsEditable(onc_source)) {
    current_ui_config_.state = ProxyPrefs::CONFIG_POLICY;
    current_ui_config_.user_modifiable = false;
  } else {
    current_ui_config_.user_modifiable = !ProxyConfigServiceImpl::IgnoreProxy(
        profile_prefs_, network.profile_path(), onc_source);
  }
}

void UIProxyConfigService::OnPreferenceChanged(const std::string& pref_name) {
  if (current_ui_network_guid_.empty())
    return;
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          current_ui_network_guid_);
  if (!network)
    return;
  UpdateFromPrefs(current_ui_network_guid_);
  NetworkHandler::Get()
      ->network_state_handler()
      ->SendUpdateNotificationForNetwork(network->path());
}

}  // namespace chromeos
