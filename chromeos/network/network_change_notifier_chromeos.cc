// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/basictypes.h"
#include "base/bind.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_change_notifier_chromeos.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "net/base/network_change_notifier.h"
#include "net/dns/dns_config_service_posix.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// DNS config services on Chrome OS are signalled by the network state handler
// rather than relying on watching files in /etc.
class NetworkChangeNotifierChromeos::DnsConfigService
    : public net::internal::DnsConfigServicePosix {
 public:
  DnsConfigService();
  virtual ~DnsConfigService();

  // net::internal::DnsConfigService() overrides.
  virtual bool StartWatching() OVERRIDE;

  virtual void OnNetworkChange();
};

NetworkChangeNotifierChromeos::DnsConfigService::DnsConfigService() {
}

NetworkChangeNotifierChromeos::DnsConfigService::~DnsConfigService() {
}

bool NetworkChangeNotifierChromeos::DnsConfigService::StartWatching() {
  // DNS config changes are handled and notified by the network state handlers.
  return true;
}

void NetworkChangeNotifierChromeos::DnsConfigService::OnNetworkChange() {
  InvalidateConfig();
  InvalidateHosts();
  ReadNow();
}

NetworkChangeNotifierChromeos::NetworkChangeNotifierChromeos()
    : NetworkChangeNotifier(NetworkChangeCalculatorParamsChromeos()),
      connection_type_(CONNECTION_NONE) {
}

NetworkChangeNotifierChromeos::~NetworkChangeNotifierChromeos() {
}

void NetworkChangeNotifierChromeos::Initialize() {
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
  NetworkStateHandler::Get()->AddObserver(this);

  dns_config_service_.reset(new DnsConfigService());
  dns_config_service_->WatchConfig(
      base::Bind(net::NetworkChangeNotifier::SetDnsConfig));

  // Update initial connection state.
  DefaultNetworkChanged(NetworkStateHandler::Get()->DefaultNetwork());
}

void NetworkChangeNotifierChromeos::Shutdown() {
  dns_config_service_.reset();
  if (NetworkStateHandler::Get())
    NetworkStateHandler::Get()->RemoveObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

net::NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierChromeos::GetCurrentConnectionType() const {
  return connection_type_;
}

void NetworkChangeNotifierChromeos::SystemResumed(
    const base::TimeDelta& sleep_duration) {
  // Force invalidation of network resources on resume.
  NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
}


void NetworkChangeNotifierChromeos::DefaultNetworkChanged(
    const chromeos::NetworkState* default_network) {
  bool connection_type_changed = false;
  bool ip_address_changed = false;
  bool dns_changed = false;

  UpdateState(default_network, &connection_type_changed,
              &ip_address_changed, &dns_changed);

  if (connection_type_changed)
    NetworkChangeNotifier::NotifyObserversOfConnectionTypeChange();
  if (ip_address_changed)
    NetworkChangeNotifier::NotifyObserversOfIPAddressChange();
  if (dns_changed)
    dns_config_service_->OnNetworkChange();
}

void NetworkChangeNotifierChromeos::UpdateState(
    const chromeos::NetworkState* default_network,
    bool* connection_type_changed,
    bool* ip_address_changed,
    bool* dns_changed) {
  *connection_type_changed = false;
  *ip_address_changed = false;
  *dns_changed = false;
  // TODO(gauravsh): DNS changes will be detected once ip config
  // support is hooked into NetworkStateHandler. For now,
  // we report a DNS change on changes to the default network (including
  // loss).
  if (!default_network || !default_network->IsConnectedState()) {
    // If we lost a default network, we must update our state and notify
    // observers, otherwise we have nothing do. (Under normal circumstances,
    // we should never get duplicate no default network notifications).
    if (connection_type_ != CONNECTION_NONE) {
      *ip_address_changed = true;
      *dns_changed = true;
      *connection_type_changed = true;
      connection_type_ = CONNECTION_NONE;
      service_path_.clear();
      ip_address_.clear();
    }
    return;
  }

  // We do have a default network and it is connected.
  net::NetworkChangeNotifier::ConnectionType new_connection_type =
      ConnectionTypeFromShill(default_network->type(),
                              default_network->technology());
  if (new_connection_type != connection_type_) {
    VLOG(1) << "Connection type changed from " << connection_type_ << " -> "
            << new_connection_type;
    *connection_type_changed = true;
    *dns_changed = true;
  }
  if (default_network->path() != service_path_ ||
      default_network->ip_address() != ip_address_) {
    VLOG(1) << "Service path changed from " << service_path_ << " -> "
            << default_network->path();
    VLOG(1) << "IP Address changed from " << ip_address_ << " -> "
            << default_network->ip_address();
    *ip_address_changed = true;
    *dns_changed = true;
  }
  connection_type_ = new_connection_type;
  service_path_ = default_network->path();
  ip_address_ = default_network->ip_address();
}

// static
net::NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierChromeos::ConnectionTypeFromShill(
    const std::string& type, const std::string& technology) {
  if (type == flimflam::kTypeEthernet)
    return CONNECTION_ETHERNET;
  else if (type == flimflam::kTypeWifi)
    return CONNECTION_WIFI;
  else if (type == flimflam::kTypeWimax)
    return CONNECTION_4G;

  if (type != flimflam::kTypeCellular)
    return CONNECTION_UNKNOWN;

  // For cellular types, mapping depends on the technology.
  if (technology == flimflam::kNetworkTechnologyEvdo ||
      technology == flimflam::kNetworkTechnologyGsm ||
      technology == flimflam::kNetworkTechnologyUmts ||
      technology == flimflam::kNetworkTechnologyHspa) {
    return CONNECTION_3G;
  } else if (technology == flimflam::kNetworkTechnologyHspaPlus ||
             technology == flimflam::kNetworkTechnologyLte ||
             technology == flimflam::kNetworkTechnologyLteAdvanced) {
    return CONNECTION_4G;
  } else {
    return CONNECTION_2G;  // Default cellular type is 2G.
  }
}

// static
net::NetworkChangeNotifier::NetworkChangeCalculatorParams
NetworkChangeNotifierChromeos::NetworkChangeCalculatorParamsChromeos() {
  NetworkChangeCalculatorParams params;
  // Delay values arrived at by simple experimentation and adjusted so as to
  // produce a single signal when switching between network connections.
  params.ip_address_offline_delay_ = base::TimeDelta::FromMilliseconds(4000);
  params.ip_address_online_delay_ = base::TimeDelta::FromMilliseconds(1000);
  params.connection_type_offline_delay_ =
      base::TimeDelta::FromMilliseconds(500);
  params.connection_type_online_delay_ = base::TimeDelta::FromMilliseconds(500);
  return params;
}

}  // namespace chromeos

