// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_change_notifier_chromeos.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "net/dns/dns_config_service_posix.h"

using content::BrowserThread;

namespace {

// Delay for online change notification reporting.
const int kOnlineNotificationDelayMS = 500;
const int kInitialNotificationCheckDelayMS = 1000;

bool IsOnline(chromeos::ConnectionState state) {
  return state == chromeos::STATE_ONLINE ||
         state == chromeos::STATE_PORTAL;
}

}

namespace chromeos {

class NetworkChangeNotifierChromeos::DnsConfigServiceChromeos
    : public net::internal::DnsConfigServicePosix {
 public:
  DnsConfigServiceChromeos() {}

  virtual ~DnsConfigServiceChromeos() {}

  // net::DnsConfigServicePosix:
  virtual bool StartWatching() OVERRIDE {
    // Notifications from NetworkLibrary are sent to
    // NetworkChangeNotifierChromeos.
    return true;
  }

  void OnNetworkChange() {
    InvalidateConfig();
    InvalidateHosts();
    ReadNow();
  }
};

NetworkChangeNotifierChromeos::NetworkChangeNotifierChromeos()
    : has_active_network_(false),
      connection_state_(chromeos::STATE_UNKNOWN),
      connection_type_(CONNECTION_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  BrowserThread::PostDelayedTask(
         BrowserThread::UI, FROM_HERE,
         base::Bind(
             &NetworkChangeNotifierChromeos::UpdateInitialState, this),
         base::TimeDelta::FromMilliseconds(kInitialNotificationCheckDelayMS));
}

NetworkChangeNotifierChromeos::~NetworkChangeNotifierChromeos() {
}

void NetworkChangeNotifierChromeos::Init() {
  chromeos::NetworkLibrary* network_library =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  network_library->AddNetworkManagerObserver(this);

  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);

  dns_config_service_.reset(new DnsConfigServiceChromeos());
  dns_config_service_->WatchConfig(
      base::Bind(NetworkChangeNotifier::SetDnsConfig));

  UpdateNetworkState(network_library);
}

void NetworkChangeNotifierChromeos::Shutdown() {
  weak_factory_.InvalidateWeakPtrs();

  dns_config_service_.reset();

  if (!chromeos::CrosLibrary::Get())
    return;

  chromeos::NetworkLibrary* lib =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  lib->RemoveNetworkManagerObserver(this);
  lib->RemoveObserverForAllNetworks(this);

  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
}

void NetworkChangeNotifierChromeos::PowerChanged(
    const PowerSupplyStatus& status) {
}

void NetworkChangeNotifierChromeos::SystemResumed() {
  // Force invalidation of various net resources on system resume.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &NetworkChangeNotifier::NotifyObserversOfIPAddressChange));
}


void NetworkChangeNotifierChromeos::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  UpdateNetworkState(cros);
}

net::NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierChromeos::GetCurrentConnectionType() const {
  return connection_type_;
}

void NetworkChangeNotifierChromeos::OnNetworkChanged(
    chromeos::NetworkLibrary* cros,
    const chromeos::Network* network) {
  CHECK(network);

  // Active network changed?
  if (network->service_path() != service_path_)
    UpdateNetworkState(cros);
  else
    UpdateConnectivityState(network);
}

void NetworkChangeNotifierChromeos::UpdateNetworkState(
    chromeos::NetworkLibrary* lib) {
  const chromeos::Network* network = lib->active_network();

  if (network) {
    VLOG(1) << "UpdateNetworkState: " << network->name()
            << ", type= " << network->type()
            << ", device= " << network->device_path()
            << ", state= " << network->state();
  }

  // Check if active network was added, removed or changed.
  if ((!network && has_active_network_) ||
      (network && (!has_active_network_ ||
                   network->service_path() != service_path_ ||
                   network->ip_address() != ip_address_))) {
    if (has_active_network_)
      lib->RemoveObserverForAllNetworks(this);
    if (!network) {
      has_active_network_ = false;
      service_path_.clear();
      ip_address_.clear();
    } else {
      has_active_network_ = true;
      service_path_ = network->service_path();
      ip_address_ = network->ip_address();
    }
    // TODO(szym): detect user DNS changes. http://crbug.com/148394
    dns_config_service_->OnNetworkChange();
    UpdateConnectivityState(network);
    // If there is an active network, add observer to track its changes.
    if (network)
      lib->AddNetworkObserver(network->service_path(), this);

    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &NetworkChangeNotifier::NotifyObserversOfIPAddressChange));
  }
}

void NetworkChangeNotifierChromeos::UpdateConnectivityState(
      const chromeos::Network* network) {
  if (network) {
    VLOG(1) << "UpdateConnectivityState: " << network->name()
            << ", type= " << network->type()
            << ", device= " << network->device_path()
            << ", state= " << network->state()
            << ", connect= " << connection_state_
            << ", type= " << connection_type_;
  }

  // We don't care about all transitions of ConnectionState.  OnlineStateChange
  // notification should trigger if ConnectionType is changed.
  chromeos::ConnectionState new_connection_state =
      network ? network->connection_state() : chromeos::STATE_UNKNOWN;

  ConnectionType prev_connection_type = connection_type_;
  ConnectionType new_connection_type = GetNetworkConnectionType(network);

  bool is_online = (new_connection_state == chromeos::STATE_ONLINE);
  bool was_online = (connection_state_ == chromeos::STATE_ONLINE);
  bool is_portal = (new_connection_state == chromeos::STATE_PORTAL);
  bool was_portal = (connection_state_ == chromeos::STATE_PORTAL);
  VLOG(2) << " UpdateConnectivityState2: "
          << "new_cs = " << new_connection_state
          << ", is_online = " << is_online
          << ", was_online = " << was_online
          << ", is_portal = " << is_portal
          << ", was_portal = " << was_portal;
  connection_state_ = new_connection_state;
  connection_type_ = new_connection_type;
  if (new_connection_type != prev_connection_type) {
    VLOG(1) << "UpdateConnectivityState3: "
            << "prev_connection_type = " << prev_connection_type
            << ", new_connection_type = " << new_connection_type;
    ReportConnectionChange();
  }
  VLOG(2) << " UpdateConnectivityState4: "
          << "new_cs = " << new_connection_state
          << ", end_cs_ = " << connection_state_
          << "prev_type = " << prev_connection_type
          << ", new_type_ = " << new_connection_type;
}

void NetworkChangeNotifierChromeos::ReportConnectionChange() {
  if (weak_factory_.HasWeakPtrs()) {
    // If we have a pending task, cancel it.
    DVLOG(1) << "ReportConnectionChange: has pending task";
    weak_factory_.InvalidateWeakPtrs();
    DVLOG(1) << "ReportConnectionChange: canceled pending task";
  }
  // Posting task with delay allows us to cancel it when connection type is
  // changed frequently. This should help us avoid transient edge reporting
  // while switching between connection types (e.g. ethernet->wifi).
  BrowserThread::PostDelayedTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &NetworkChangeNotifierChromeos::ReportConnectionChangeOnUIThread,
          weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kOnlineNotificationDelayMS));
}

void NetworkChangeNotifierChromeos::ReportConnectionChangeOnUIThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
         &NetworkChangeNotifierChromeos::
             NotifyObserversOfConnectionTypeChange));
}

// static
void NetworkChangeNotifierChromeos::UpdateInitialState(
    NetworkChangeNotifierChromeos* self) {
  chromeos::NetworkLibrary* net =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  self->UpdateNetworkState(net);
}

// static
net::NetworkChangeNotifier::ConnectionType
NetworkChangeNotifierChromeos::GetNetworkConnectionType(
    const chromeos::Network* network) {
  if (!network || !IsOnline(network->connection_state()))
    return net::NetworkChangeNotifier::CONNECTION_NONE;

  switch (network->type()) {
    case chromeos::TYPE_ETHERNET:
      return CONNECTION_ETHERNET;
    case chromeos::TYPE_WIFI:
      return CONNECTION_WIFI;
    case chromeos::TYPE_WIMAX:
      return CONNECTION_4G;
    case chromeos::TYPE_CELLULAR:
      switch (static_cast<const chromeos::CellularNetwork*>(
          network)->network_technology()) {
        case chromeos::NETWORK_TECHNOLOGY_UNKNOWN:
        case chromeos::NETWORK_TECHNOLOGY_1XRTT:
        case chromeos::NETWORK_TECHNOLOGY_GPRS:
        case chromeos::NETWORK_TECHNOLOGY_EDGE:
          return CONNECTION_2G;
        case chromeos::NETWORK_TECHNOLOGY_GSM:
        case chromeos::NETWORK_TECHNOLOGY_UMTS:
        case chromeos::NETWORK_TECHNOLOGY_EVDO:
        case chromeos::NETWORK_TECHNOLOGY_HSPA:
          return CONNECTION_3G;
        case chromeos::NETWORK_TECHNOLOGY_HSPA_PLUS:
        case chromeos::NETWORK_TECHNOLOGY_LTE:
        case chromeos::NETWORK_TECHNOLOGY_LTE_ADVANCED:
          return CONNECTION_4G;
      }
    case chromeos::TYPE_BLUETOOTH:
    case chromeos::TYPE_VPN:
    case chromeos::TYPE_UNKNOWN:
      break;
  }
  return net::NetworkChangeNotifier::CONNECTION_UNKNOWN;
}

}  // namespace chromeos
