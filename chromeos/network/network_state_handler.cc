// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/guid.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/tether_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

bool ConnectionStateChanged(NetworkState* network,
                            const std::string& prev_connection_state,
                            bool prev_is_captive_portal) {
  return ((network->connection_state() != prev_connection_state) &&
          !((network->connection_state() == shill::kStateIdle) &&
            prev_connection_state.empty())) ||
         (network->is_captive_portal() != prev_is_captive_portal);
}

std::string GetManagedStateLogType(const ManagedState* state) {
  switch (state->managed_type()) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      return "Network";
    case ManagedState::MANAGED_TYPE_DEVICE:
      return "Device";
  }
  NOTREACHED();
  return "";
}

std::string GetLogName(const ManagedState* state) {
  if (!state)
    return "None";
  return base::StringPrintf("%s (%s)", state->name().c_str(),
                            state->path().c_str());
}

std::string ValueAsString(const base::Value& value) {
  std::string vstr;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_OMIT_BINARY_VALUES, &vstr);
  return vstr.empty() ? "''" : vstr;
}

}  // namespace

const char NetworkStateHandler::kDefaultCheckPortalList[] =
    "ethernet,wifi,cellular";

NetworkStateHandler::NetworkStateHandler() {}

NetworkStateHandler::~NetworkStateHandler() {
  // Normally Shutdown() will get called in ~NetworkHandler, however unit
  // tests do not use that class so this needs to call Shutdown when we
  // destry the class.
  if (!did_shutdown_)
    Shutdown();
}

void NetworkStateHandler::Shutdown() {
  DCHECK(!did_shutdown_);
  did_shutdown_ = true;
  for (auto& observer : observers_)
    observer.OnShuttingDown();
}

void NetworkStateHandler::InitShillPropertyHandler() {
  shill_property_handler_ =
      base::MakeUnique<internal::ShillPropertyHandler>(this);
  shill_property_handler_->Init();
}

// static
std::unique_ptr<NetworkStateHandler> NetworkStateHandler::InitializeForTest() {
  auto handler = base::WrapUnique(new NetworkStateHandler());
  handler->InitShillPropertyHandler();
  return handler;
}

void NetworkStateHandler::AddObserver(
    NetworkStateHandlerObserver* observer,
    const tracked_objects::Location& from_here) {
  observers_.AddObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::AddObserver: 0x%p", observer));
}

void NetworkStateHandler::RemoveObserver(
    NetworkStateHandlerObserver* observer,
    const tracked_objects::Location& from_here) {
  observers_.RemoveObserver(observer);
  device_event_log::AddEntry(
      from_here.file_name(), from_here.line_number(),
      device_event_log::LOG_TYPE_NETWORK, device_event_log::LOG_LEVEL_DEBUG,
      base::StringPrintf("NetworkStateHandler::RemoveObserver: 0x%p",
                         observer));
}

NetworkStateHandler::TechnologyState NetworkStateHandler::GetTechnologyState(
    const NetworkTypePattern& type) const {
  std::string technology = GetTechnologyForType(type);

  if (technology == kTypeTether) {
    return tether_technology_state_;
  }

  TechnologyState state;
  if (shill_property_handler_->IsTechnologyEnabled(technology))
    state = TECHNOLOGY_ENABLED;
  else if (shill_property_handler_->IsTechnologyEnabling(technology))
    state = TECHNOLOGY_ENABLING;
  else if (shill_property_handler_->IsTechnologyProhibited(technology))
    state = TECHNOLOGY_PROHIBITED;
  else if (shill_property_handler_->IsTechnologyUninitialized(technology))
    state = TECHNOLOGY_UNINITIALIZED;
  else if (shill_property_handler_->IsTechnologyAvailable(technology))
    state = TECHNOLOGY_AVAILABLE;
  else
    state = TECHNOLOGY_UNAVAILABLE;
  VLOG(2) << "GetTechnologyState: " << type.ToDebugString() << " = " << state;
  return state;
}

void NetworkStateHandler::SetTechnologyEnabled(
    const NetworkTypePattern& type,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  std::vector<std::string> technologies = GetTechnologiesForType(type);
  for (const std::string& technology : technologies) {
    if (technology == kTypeTether) {
      if (tether_technology_state_ != TECHNOLOGY_ENABLED &&
          tether_technology_state_ != TECHNOLOGY_AVAILABLE) {
        NET_LOG(ERROR) << "SetTechnologyEnabled() called for the Tether "
                       << "DeviceState, but the current state was: "
                       << tether_technology_state_;
        network_handler::RunErrorCallback(
            error_callback, kTetherDevicePath,
            NetworkConnectionHandler::kErrorEnabledOrDisabledWhenNotAvailable,
            "");
        continue;
      }

      // Tether does not exist in Shill, so set |tether_technology_state_| and
      // skip the below interactions with |shill_property_handler_|.
      tether_technology_state_ =
          enabled ? TECHNOLOGY_ENABLED : TECHNOLOGY_AVAILABLE;
      continue;
    }

    if (!shill_property_handler_->IsTechnologyAvailable(technology))
      continue;
    NET_LOG_USER("SetTechnologyEnabled",
                 base::StringPrintf("%s:%d", technology.c_str(), enabled));
    shill_property_handler_->SetTechnologyEnabled(technology, enabled,
                                                  error_callback);
  }
  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::SetTetherTechnologyState(
    TechnologyState technology_state) {
  if (tether_technology_state_ == technology_state)
    return;

  tether_technology_state_ = technology_state;
  EnsureTetherDeviceState();

  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::SetTetherScanState(bool is_scanning) {
  DeviceState* tether_device_state =
      GetModifiableDeviceState(kTetherDevicePath);
  DCHECK(tether_device_state);

  bool was_scanning = tether_device_state->scanning();
  tether_device_state->set_scanning(is_scanning);

  if (was_scanning && !is_scanning) {
    // If a scan was in progress but has completed, notify observers.
    NotifyScanCompleted(tether_device_state);
  }
}

void NetworkStateHandler::SetProhibitedTechnologies(
    const std::vector<std::string>& prohibited_technologies,
    const network_handler::ErrorCallback& error_callback) {
  // Make a copy of |prohibited_technologies| since the list may be edited
  // within this function.
  std::vector<std::string> prohibited_technologies_copy =
      prohibited_technologies;

  auto it = prohibited_technologies_copy.begin();
  while (it != prohibited_technologies_copy.end()) {
    if (*it == kTypeTether) {
      // If Tether networks are prohibited, set |tether_technology_state_| and
      // remove |kTypeTether| from the list before passing it to
      // |shill_property_handler_| below. Shill does not have a concept of
      // Tether networks, so it cannot prohibit that technology type.
      tether_technology_state_ = TECHNOLOGY_PROHIBITED;
      it = prohibited_technologies_copy.erase(it);
    } else {
      ++it;
    }
  }

  shill_property_handler_->SetProhibitedTechnologies(
      prohibited_technologies_copy, error_callback);
  // Signal Device/Technology state changed.
  NotifyDeviceListChanged();
}

const DeviceState* NetworkStateHandler::GetDeviceState(
    const std::string& device_path) const {
  const DeviceState* device = GetModifiableDeviceState(device_path);
  if (device && !device->update_received())
    return nullptr;
  return device;
}

const DeviceState* NetworkStateHandler::GetDeviceStateByType(
    const NetworkTypePattern& type) const {
  for (const auto& device : device_list_) {
    if (!device->update_received())
      continue;
    if (device->Matches(type))
      return device->AsDeviceState();
  }
  return nullptr;
}

bool NetworkStateHandler::GetScanningByType(
    const NetworkTypePattern& type) const {
  for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (!device->update_received())
      continue;
    if (device->Matches(type) && device->scanning())
      return true;
  }
  return false;
}

const NetworkState* NetworkStateHandler::GetNetworkState(
    const std::string& service_path) const {
  const NetworkState* network = GetModifiableNetworkState(service_path);
  if (network && !network->update_received())
    return nullptr;
  return network;
}

const NetworkState* NetworkStateHandler::DefaultNetwork() const {
  if (default_network_path_.empty())
    return nullptr;
  return GetNetworkState(default_network_path_);
}

const NetworkState* NetworkStateHandler::ConnectedNetworkByType(
    const NetworkTypePattern& type) const {
  const NetworkState* connected_network = nullptr;

  // Active networks are always listed first by Shill so no need to sort.
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received())
      continue;
    if (!network->IsConnectedState())
      break;  // Connected networks are listed first.
    if (network->Matches(type)) {
      connected_network = network;
      break;
    }
  }

  // Ethernet networks are prioritized over Tether networks.
  if (connected_network && connected_network->type() == shill::kTypeEthernet) {
    return connected_network;
  }

  // Tether networks are prioritized over non-Ethernet networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether())) {
    for (auto iter = tether_network_list_.begin();
         iter != tether_network_list_.end(); ++iter) {
      const NetworkState* network = (*iter)->AsNetworkState();
      DCHECK(network);
      if (network->IsConnectedState())
        return network;
    }
  }

  return connected_network;
}

const NetworkState* NetworkStateHandler::ConnectingNetworkByType(
    const NetworkTypePattern& type) const {
  const NetworkState* connecting_network = nullptr;

  // Active networks are always listed first by Shill so no need to sort.
  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || network->IsConnectedState())
      continue;
    if (!network->IsConnectingState())
      break;  // Connected and connecting networks are listed first.
    if (network->Matches(type)) {
      connecting_network = network;
      break;
    }
  }

  // Ethernet networks are prioritized over Tether networks.
  if (connecting_network &&
      connecting_network->type() == shill::kTypeEthernet) {
    return connecting_network;
  }

  // Tether networks are prioritized over non-Ethernet networks.
  if (type.MatchesPattern(NetworkTypePattern::Tether())) {
    for (auto iter = tether_network_list_.begin();
         iter != tether_network_list_.end(); ++iter) {
      const NetworkState* network = (*iter)->AsNetworkState();
      DCHECK(network);
      if (network->IsConnectingState())
        return network;
    }
  }

  return connecting_network;
}

const NetworkState* NetworkStateHandler::FirstNetworkByType(
    const NetworkTypePattern& type) {
  if (!network_list_sorted_)
    SortNetworkList();  // Sort to ensure visible networks are listed first.

  // If |type| matches Tether networks and at least one Tether network is
  // present, return the first network (since it has been sorted already).
  if (type.MatchesPattern(NetworkTypePattern::Tether()) &&
      !tether_network_list_.empty()) {
    return tether_network_list_[0]->AsNetworkState();
  }

  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received())
      continue;
    if (!network->visible())
      break;
    if (network->Matches(type))
      return network;
  }
  return nullptr;
}

std::string NetworkStateHandler::FormattedHardwareAddressForType(
    const NetworkTypePattern& type) const {
  const NetworkState* network = ConnectedNetworkByType(type);
  if (network && network->type() == kTypeTether) {
    // If this is a Tether network, get the MAC address corresponding to that
    // network instead.
    network = GetNetworkStateFromGuid(network->tether_guid());
  }
  const DeviceState* device = network ? GetDeviceState(network->device_path())
                                      : GetDeviceStateByType(type);
  if (!device || device->mac_address().empty())
    return std::string();
  return network_util::FormattedMacAddress(device->mac_address());
}

void NetworkStateHandler::GetVisibleNetworkListByType(
    const NetworkTypePattern& type,
    NetworkStateList* list) {
  GetNetworkListByType(type, false /* configured_only */,
                       true /* visible_only */, 0 /* no limit */, list);
}

void NetworkStateHandler::GetVisibleNetworkList(NetworkStateList* list) {
  GetVisibleNetworkListByType(NetworkTypePattern::Default(), list);
}

void NetworkStateHandler::GetNetworkListByType(const NetworkTypePattern& type,
                                               bool configured_only,
                                               bool visible_only,
                                               int limit,
                                               NetworkStateList* list) {
  DCHECK(list);
  list->clear();

  // Sort the network list if necessary.
  if (!network_list_sorted_)
    SortNetworkList();

  if (type.MatchesPattern(NetworkTypePattern::Tether()))
    GetTetherNetworkList(limit, list);

  int count = list->size();

  if (type.Equals(NetworkTypePattern::Tether()) ||
      (limit != 0 && count >= limit)) {
    // If only searching for Tether networks, there is no need to continue
    // searching through other network types; likewise, if the limit has already
    // been reached, there is no need to continue searching.
    return;
  }

  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->update_received() || !network->Matches(type))
      continue;
    if (configured_only && !network->IsInProfile())
      continue;
    if (visible_only && !network->visible())
      continue;
    if (network->type() == shill::kTypeEthernet) {
      // Ethernet networks should always be in front.
      list->insert(list->begin(), network);
    } else {
      list->push_back(network);
    }
    if (limit > 0 && ++count >= limit)
      break;
  }
}

void NetworkStateHandler::GetTetherNetworkList(int limit,
                                               NetworkStateList* list) {
  DCHECK(list);
  list->clear();

  if (!IsTechnologyEnabled(NetworkTypePattern::Tether()))
    return;

  int count = 0;
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    list->push_back((*iter)->AsNetworkState());
    if (limit > 0 && ++count >= limit)
      return;
  }
}

const NetworkState* NetworkStateHandler::GetNetworkStateFromServicePath(
    const std::string& service_path,
    bool configured_only) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed) {
    managed = GetModifiableManagedState(&tether_network_list_, service_path);
    if (!managed)
      return nullptr;
  }
  const NetworkState* network = managed->AsNetworkState();
  DCHECK(network);
  if (!network->update_received() ||
      (configured_only && !network->IsInProfile())) {
    return nullptr;
  }
  return network;
}

const NetworkState* NetworkStateHandler::GetNetworkStateFromGuid(
    const std::string& guid) const {
  DCHECK(!guid.empty());
  return GetModifiableNetworkStateFromGuid(guid);
}

void NetworkStateHandler::AddTetherNetworkState(const std::string& guid,
                                                const std::string& name,
                                                const std::string& carrier,
                                                int battery_percentage,
                                                int signal_strength,
                                                bool has_connected_to_host) {
  DCHECK(!guid.empty());
  DCHECK(battery_percentage >= 0 && battery_percentage <= 100);
  DCHECK(signal_strength >= 0 && signal_strength <= 100);

  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "AddTetherNetworkState() called when Tether networks "
                   << "are not enabled. Cannot add NetworkState.";
    return;
  }

  // If the network already exists, do nothing.
  if (GetNetworkStateFromGuid(guid)) {
    NET_LOG(ERROR) << "AddTetherNetworkState: " << name
                   << " called with existing guid:" << guid;
    return;
  }

  // Use the GUID as the network's service path.
  std::unique_ptr<NetworkState> tether_network_state =
      base::MakeUnique<NetworkState>(guid /* path */);

  tether_network_state->set_name(name);
  tether_network_state->set_type(kTypeTether);
  tether_network_state->SetGuid(guid);
  tether_network_state->set_visible(true);
  tether_network_state->set_update_received();
  tether_network_state->set_update_requested(false);
  tether_network_state->set_connectable(true);
  tether_network_state->set_carrier(carrier);
  tether_network_state->set_battery_percentage(battery_percentage);
  tether_network_state->set_tether_has_connected_to_host(has_connected_to_host);
  tether_network_state->set_signal_strength(signal_strength);

  tether_network_list_.push_back(std::move(tether_network_state));
  NotifyNetworkListChanged();
}

bool NetworkStateHandler::UpdateTetherNetworkProperties(
    const std::string& guid,
    const std::string& carrier,
    int battery_percentage,
    int signal_strength) {
  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "UpdateTetherNetworkProperties() called when Tether "
                   << "networks are not enabled. Cannot update.";
    return false;
  }

  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "UpdateTetherNetworkProperties(): No NetworkState for "
                   << "Tether network with GUID \"" << guid << "\".";
    return false;
  }

  tether_network_state->set_carrier(carrier);
  tether_network_state->set_battery_percentage(battery_percentage);
  tether_network_state->set_signal_strength(signal_strength);

  NotifyNetworkPropertiesUpdated(tether_network_state);
  return true;
}

bool NetworkStateHandler::SetTetherNetworkHasConnectedToHost(
    const std::string& guid) {
  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "SetTetherNetworkHasConnectedToHost(): No NetworkState "
                   << "for Tether network with GUID \"" << guid << "\".";
    return false;
  }

  if (tether_network_state->tether_has_connected_to_host()) {
    return false;
  }

  tether_network_state->set_tether_has_connected_to_host(true);

  NotifyNetworkPropertiesUpdated(tether_network_state);
  return true;
}

bool NetworkStateHandler::RemoveTetherNetworkState(const std::string& guid) {
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    if (iter->get()->AsNetworkState()->guid() == guid) {
      NetworkState* wifi_network = GetModifiableNetworkStateFromGuid(
          iter->get()->AsNetworkState()->tether_guid());
      if (wifi_network)
        wifi_network->set_tether_guid(std::string());

      tether_network_list_.erase(iter);
      NotifyNetworkListChanged();

      return true;
    }
  }
  return false;
}

bool NetworkStateHandler::DisassociateTetherNetworkStateFromWifiNetwork(
    const std::string& tether_network_guid) {
  NetworkState* tether_network_state =
      GetModifiableNetworkStateFromGuid(tether_network_guid);

  if (!tether_network_state) {
    NET_LOG(ERROR) << "DisassociateTetherNetworkStateWithWifiNetwork(): Tether "
                   << "network with ID " << tether_network_guid
                   << " not registered; could not remove association.";
    return false;
  }

  std::string wifi_network_guid = tether_network_state->tether_guid();
  NetworkState* wifi_network_state =
      GetModifiableNetworkStateFromGuid(wifi_network_guid);

  if (!wifi_network_state) {
    NET_LOG(ERROR) << "DisassociateTetherNetworkStateWithWifiNetwork(): Wi-Fi "
                   << "network with ID " << wifi_network_guid
                   << " not registered; could not remove association.";
    return false;
  }

  if (wifi_network_state->tether_guid().empty() &&
      tether_network_state->tether_guid().empty()) {
    return true;
  }

  wifi_network_state->set_tether_guid(std::string());
  tether_network_state->set_tether_guid(std::string());

  NotifyNetworkPropertiesUpdated(wifi_network_state);
  NotifyNetworkPropertiesUpdated(tether_network_state);

  return true;
}

bool NetworkStateHandler::AssociateTetherNetworkStateWithWifiNetwork(
    const std::string& tether_network_guid,
    const std::string& wifi_network_guid) {
  if (tether_technology_state_ != TECHNOLOGY_ENABLED) {
    NET_LOG(ERROR) << "AssociateTetherNetworkStateWithWifiNetwork() called "
                   << "when Tether networks are not enabled. Cannot "
                   << "associate.";
    return false;
  }

  NetworkState* tether_network_state =
      GetModifiableNetworkStateFromGuid(tether_network_guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "Tether network does not exist: " << tether_network_guid;
    return false;
  }
  if (!NetworkTypePattern::Tether().MatchesType(tether_network_state->type())) {
    NET_LOG(ERROR) << "Network is not a Tether network: "
                   << tether_network_guid;
    return false;
  }

  NetworkState* wifi_network_state =
      GetModifiableNetworkStateFromGuid(wifi_network_guid);
  if (!wifi_network_state) {
    NET_LOG(ERROR) << "Wi-Fi Network does not exist: " << wifi_network_guid;
    return false;
  }
  if (!NetworkTypePattern::WiFi().MatchesType(wifi_network_state->type())) {
    NET_LOG(ERROR) << "Network is not a W-Fi network: " << wifi_network_guid;
    return false;
  }

  if (wifi_network_state->tether_guid() == tether_network_guid &&
      tether_network_state->tether_guid() == wifi_network_guid) {
    return true;
  }

  tether_network_state->set_tether_guid(wifi_network_guid);
  wifi_network_state->set_tether_guid(tether_network_guid);

  NotifyNetworkPropertiesUpdated(wifi_network_state);
  NotifyNetworkPropertiesUpdated(tether_network_state);

  return true;
}

void NetworkStateHandler::SetTetherNetworkStateDisconnected(
    const std::string& guid) {
  SetTetherNetworkStateConnectionState(guid, shill::kStateDisconnect);
}

void NetworkStateHandler::SetTetherNetworkStateConnecting(
    const std::string& guid) {
  // The default network should only be set if there currently is no default
  // network. Otherwise, the default network should not change unless the
  // connection completes successfully and the newly-connected network is
  // prioritized higher than the existing default network. Note that, in
  // general, a connected Ethernet network is still considered the default
  // network even if a Tether or Wi-Fi network becomes connected.
  if (default_network_path_.empty()) {
    NET_LOG(EVENT) << "Connecting to Tether network when there is currently no "
                   << "default network; setting as new default network. GUID: "
                   << guid;
    default_network_path_ = guid;
  }

  SetTetherNetworkStateConnectionState(guid, shill::kStateConfiguration);
}

void NetworkStateHandler::SetTetherNetworkStateConnected(
    const std::string& guid) {
  // Being connected implies that AssociateTetherNetworkStateWithWifiNetwork()
  // was already called, so ensure that the association is still intact.
  DCHECK(GetNetworkStateFromGuid(GetNetworkStateFromGuid(guid)->tether_guid())
             ->tether_guid() == guid);

  // At this point, there should be a default network set.
  DCHECK(!default_network_path_.empty());

  SetTetherNetworkStateConnectionState(guid, shill::kStateOnline);
}

void NetworkStateHandler::SetTetherNetworkStateConnectionState(
    const std::string& guid,
    const std::string& connection_state) {
  NetworkState* tether_network_state = GetModifiableNetworkStateFromGuid(guid);
  if (!tether_network_state) {
    NET_LOG(ERROR) << "SetTetherNetworkStateConnectionState: Tether network "
                   << "not found: " << guid;
    return;
  }

  DCHECK(
      NetworkTypePattern::Tether().MatchesType(tether_network_state->type()));

  std::string prev_connection_state = tether_network_state->connection_state();
  tether_network_state->set_connection_state(connection_state);
  DCHECK(!tether_network_state->is_captive_portal());

  if (ConnectionStateChanged(tether_network_state, prev_connection_state,
                             false /* prev_is_captive_portal */)) {
    NET_LOG(EVENT) << "Changing connection state for Tether network with GUID "
                   << guid << ". Old state: " << prev_connection_state << ", "
                   << "New state: " << connection_state;

    OnNetworkConnectionStateChanged(tether_network_state);
    NotifyNetworkPropertiesUpdated(tether_network_state);
  }
}

void NetworkStateHandler::EnsureTetherDeviceState() {
  bool should_be_present =
      tether_technology_state_ != TechnologyState::TECHNOLOGY_UNAVAILABLE;

  for (auto it = device_list_.begin(); it < device_list_.end(); ++it) {
    std::string path = (*it)->path();
    if (path == kTetherDevicePath) {
      // If the Tether DeviceState is in the list and it should not be, remove
      // it and return. If it is in the list and it should be, the list is
      // already valid, so return without removing it.
      if (!should_be_present)
        device_list_.erase(it);
      return;
    }
  }

  if (!should_be_present) {
    // If the Tether DeviceState was not in the list and it should not be, the
    // list is already valid, so return.
    return;
  }

  // The Tether DeviceState is not present in the list, but it should be. Since
  // Tether networks are not recognized by Shill, they will never receive an
  // update, so set properties on the state here.
  std::unique_ptr<ManagedState> tether_device_state = ManagedState::Create(
      ManagedState::ManagedType::MANAGED_TYPE_DEVICE, kTetherDevicePath);
  tether_device_state->set_update_received();
  tether_device_state->set_update_requested(false);
  tether_device_state->set_name(kTetherDeviceName);
  tether_device_state->set_type(kTypeTether);

  device_list_.push_back(std::move(tether_device_state));
}

void NetworkStateHandler::GetDeviceList(DeviceStateList* list) const {
  GetDeviceListByType(NetworkTypePattern::Default(), list);
}

void NetworkStateHandler::GetDeviceListByType(const NetworkTypePattern& type,
                                              DeviceStateList* list) const {
  DCHECK(list);
  list->clear();

  for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (device->update_received() && device->Matches(type))
      list->push_back(device);
  }
}

void NetworkStateHandler::RequestScan() {
  NET_LOG_USER("RequestScan", "");
  shill_property_handler_->RequestScan();
  NotifyScanRequested();
}

void NetworkStateHandler::RequestUpdateForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network)
    network->set_update_requested(true);
  NET_LOG_EVENT("RequestUpdate", service_path);
  shill_property_handler_->RequestProperties(ManagedState::MANAGED_TYPE_NETWORK,
                                             service_path);
}

void NetworkStateHandler::SendUpdateNotificationForNetwork(
    const std::string& service_path) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network)
    return;
  NotifyNetworkPropertiesUpdated(network);
}

void NetworkStateHandler::ClearLastErrorForNetwork(
    const std::string& service_path) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (network)
    network->clear_last_error();
}

void NetworkStateHandler::SetCheckPortalList(
    const std::string& check_portal_list) {
  NET_LOG_EVENT("SetCheckPortalList", check_portal_list);
  shill_property_handler_->SetCheckPortalList(check_portal_list);
}

void NetworkStateHandler::SetWakeOnLanEnabled(bool enabled) {
  NET_LOG_EVENT("SetWakeOnLanEnabled", enabled ? "true" : "false");
  shill_property_handler_->SetWakeOnLanEnabled(enabled);
}

void NetworkStateHandler::SetNetworkThrottlingStatus(
    bool enabled,
    uint32_t upload_rate_kbits,
    uint32_t download_rate_kbits) {
  NET_LOG_EVENT("SetNetworkThrottlingStatus",
                enabled ? ("true :" + base::IntToString(upload_rate_kbits) +
                           ", " + base::IntToString(download_rate_kbits))
                        : "false");
  shill_property_handler_->SetNetworkThrottlingStatus(
      enabled, upload_rate_kbits, download_rate_kbits);
}

const NetworkState* NetworkStateHandler::GetEAPForEthernet(
    const std::string& service_path) {
  const NetworkState* network = GetNetworkState(service_path);
  if (!network) {
    NET_LOG_ERROR("GetEAPForEthernet", "Unknown service path " + service_path);
    return nullptr;
  }
  if (network->type() != shill::kTypeEthernet) {
    NET_LOG_ERROR("GetEAPForEthernet", "Not of type Ethernet: " + service_path);
    return nullptr;
  }
  if (!network->IsConnectedState())
    return nullptr;

  // The same EAP service is shared for all ethernet services/devices.
  // However EAP is used/enabled per device and only if the connection was
  // successfully established.
  const DeviceState* device = GetDeviceState(network->device_path());
  if (!device) {
    NET_LOG(ERROR) << "GetEAPForEthernet: Unknown device "
                   << network->device_path()
                   << " for connected ethernet service: " << service_path;
    return nullptr;
  }
  if (!device->eap_authentication_completed())
    return nullptr;

  NetworkStateList list;
  GetNetworkListByType(NetworkTypePattern::Primitive(shill::kTypeEthernetEap),
                       true /* configured_only */, false /* visible_only */,
                       1 /* limit */, &list);
  if (list.empty()) {
    NET_LOG_ERROR("GetEAPForEthernet",
                  base::StringPrintf(
                      "Ethernet service %s connected using EAP, but no "
                      "EAP service found.",
                      service_path.c_str()));
    return nullptr;
  }
  return list.front();
}

void NetworkStateHandler::SetLastErrorForTest(const std::string& service_path,
                                              const std::string& error) {
  NetworkState* network_state = GetModifiableNetworkState(service_path);
  if (!network_state) {
    NET_LOG(ERROR) << "No matching NetworkState for: " << service_path;
    return;
  }
  network_state->last_error_ = error;
}

//------------------------------------------------------------------------------
// ShillPropertyHandler::Delegate overrides

void NetworkStateHandler::UpdateManagedList(ManagedState::ManagedType type,
                                            const base::ListValue& entries) {
  ManagedStateList* managed_list = GetManagedList(type);
  NET_LOG_DEBUG("UpdateManagedList: " + ManagedState::TypeToString(type),
                base::StringPrintf("%" PRIuS, entries.GetSize()));
  // Create a map of existing entries. Assumes all entries in |managed_list|
  // are unique.
  std::map<std::string, std::unique_ptr<ManagedState>> managed_map;
  for (auto& item : *managed_list) {
    std::string path = item->path();
    DCHECK(!base::ContainsKey(managed_map, path));
    managed_map[path] = std::move(item);
  }
  // Clear the list (objects are temporarily owned by managed_map).
  managed_list->clear();
  // Updates managed_list and request updates for new entries.
  std::set<std::string> list_entries;
  for (auto& iter : entries) {
    std::string path;
    iter.GetAsString(&path);
    if (path.empty() || path == shill::kFlimflamServicePath) {
      NET_LOG_ERROR(base::StringPrintf("Bad path in list:%d", type), path);
      continue;
    }
    auto found = managed_map.find(path);
    if (found == managed_map.end()) {
      if (list_entries.count(path) != 0) {
        NET_LOG_ERROR("Duplicate entry in list", path);
        continue;
      }
      managed_list->push_back(ManagedState::Create(type, path));
    } else {
      managed_list->push_back(std::move(found->second));
      managed_map.erase(found);
    }
    list_entries.insert(path);
  }

  if (type == ManagedState::ManagedType::MANAGED_TYPE_DEVICE) {
    // Also move the Tether DeviceState if it exists. This will not happen as
    // part of the loop above since |entries| will never contain the Tether
    // path.
    auto iter = managed_map.find(kTetherDevicePath);
    if (iter != managed_map.end()) {
      managed_list->push_back(std::move(iter->second));
      managed_map.erase(iter);
    }
  }

  if (type != ManagedState::ManagedType::MANAGED_TYPE_NETWORK)
    return;

  // Remove associations Tether NetworkStates had with now removed Wi-Fi
  // NetworkStates.
  for (auto& iter : managed_map) {
    if (!iter.second->Matches(NetworkTypePattern::WiFi()))
      continue;

    NetworkState* tether_network = GetModifiableNetworkStateFromGuid(
        iter.second->AsNetworkState()->tether_guid());
    if (tether_network)
      tether_network->set_tether_guid(std::string());
  }
}

void NetworkStateHandler::ProfileListChanged() {
  NET_LOG_EVENT("ProfileListChanged", "Re-Requesting Network Properties");
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    shill_property_handler_->RequestProperties(
        ManagedState::MANAGED_TYPE_NETWORK, network->path());
  }
}

void NetworkStateHandler::UpdateManagedStateProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::DictionaryValue& properties) {
  ManagedStateList* managed_list = GetManagedList(type);
  ManagedState* managed = GetModifiableManagedState(managed_list, path);
  if (!managed) {
    // The network has been removed from the list of networks.
    NET_LOG_DEBUG("UpdateManagedStateProperties: Not found", path);
    return;
  }
  managed->set_update_received();

  std::string desc = GetManagedStateLogType(managed) + " Properties Received";
  NET_LOG_DEBUG(desc, GetLogName(managed));

  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    UpdateNetworkStateProperties(managed->AsNetworkState(), properties);
  } else {
    // Device
    for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
         iter.Advance()) {
      managed->PropertyChanged(iter.key(), iter.value());
    }
    managed->InitialPropertiesReceived(properties);
  }
  managed->set_update_requested(false);
}

void NetworkStateHandler::UpdateNetworkStateProperties(
    NetworkState* network,
    const base::DictionaryValue& properties) {
  DCHECK(network);
  bool network_property_updated = false;
  std::string prev_connection_state = network->connection_state();
  bool prev_is_captive_portal = network->is_captive_portal();
  for (base::DictionaryValue::Iterator iter(properties); !iter.IsAtEnd();
       iter.Advance()) {
    if (network->PropertyChanged(iter.key(), iter.value()))
      network_property_updated = true;
  }
  network_property_updated |= network->InitialPropertiesReceived(properties);
  UpdateGuid(network);
  network_list_sorted_ = false;

  // Notify observers of NetworkState changes.
  if (network_property_updated || network->update_requested()) {
    // Signal connection state changed after all properties have been updated.
    if (ConnectionStateChanged(network, prev_connection_state,
                               prev_is_captive_portal)) {
      OnNetworkConnectionStateChanged(network);
    }
    NET_LOG_EVENT("NetworkPropertiesUpdated", GetLogName(network));
    NotifyNetworkPropertiesUpdated(network);
  }
}

void NetworkStateHandler::UpdateNetworkServiceProperty(
    const std::string& service_path,
    const std::string& key,
    const base::Value& value) {
  SCOPED_NET_LOG_IF_SLOW();
  bool changed = false;
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network)
    return;
  std::string prev_connection_state = network->connection_state();
  bool prev_is_captive_portal = network->is_captive_portal();
  std::string prev_profile_path = network->profile_path();
  changed |= network->PropertyChanged(key, value);
  if (!changed)
    return;

  if (key == shill::kStateProperty || key == shill::kVisibleProperty) {
    network_list_sorted_ = false;
    if (ConnectionStateChanged(network, prev_connection_state,
                               prev_is_captive_portal)) {
      OnNetworkConnectionStateChanged(network);
      // If the connection state changes, other properties such as IPConfig
      // may have changed, so request a full update.
      RequestUpdateForNetwork(service_path);
    }
  } else {
    std::string value_str;
    value.GetAsString(&value_str);
    // Some property changes are noisy and not interesting:
    // * Wifi SignalStrength
    // * WifiFrequencyList updates
    // * Device property changes to "/" (occurs before a service is removed)
    if (key != shill::kSignalStrengthProperty &&
        key != shill::kWifiFrequencyListProperty &&
        (key != shill::kDeviceProperty || value_str != "/")) {
      std::string log_event = "NetworkPropertyUpdated";
      // Trigger a default network update for interesting changes only.
      if (network->path() == default_network_path_) {
        NotifyDefaultNetworkChanged(network);
        log_event = "Default" + log_event;
      }
      // Log event.
      std::string detail = network->name() + "." + key;
      detail += " = " + ValueAsString(value);
      device_event_log::LogLevel log_level;
      if (key == shill::kErrorProperty || key == shill::kErrorDetailsProperty) {
        log_level = device_event_log::LOG_LEVEL_ERROR;
      } else {
        log_level = device_event_log::LOG_LEVEL_EVENT;
      }
      NET_LOG_LEVEL(log_level, log_event, detail);
    }
  }

  // All property updates signal 'NetworkPropertiesUpdated'.
  NotifyNetworkPropertiesUpdated(network);

  // If added to a Profile, request a full update so that a NetworkState
  // gets created.
  if (prev_profile_path.empty() && !network->profile_path().empty())
    RequestUpdateForNetwork(service_path);
}

void NetworkStateHandler::UpdateDeviceProperty(const std::string& device_path,
                                               const std::string& key,
                                               const base::Value& value) {
  SCOPED_NET_LOG_IF_SLOW();
  DeviceState* device = GetModifiableDeviceState(device_path);
  if (!device)
    return;
  if (!device->PropertyChanged(key, value))
    return;

  std::string detail = device->name() + "." + key;
  detail += " = " + ValueAsString(value);
  NET_LOG_EVENT("DevicePropertyUpdated", detail);

  NotifyDeviceListChanged();
  NotifyDevicePropertiesUpdated(device);

  if (key == shill::kScanningProperty && device->scanning() == false) {
    NotifyScanCompleted(device);
  }
  if (key == shill::kEapAuthenticationCompletedProperty) {
    // Notify a change for each Ethernet service using this device.
    NetworkStateList ethernet_services;
    GetNetworkListByType(NetworkTypePattern::Ethernet(),
                         false /* configured_only */, false /* visible_only */,
                         0 /* no limit */, &ethernet_services);
    for (NetworkStateList::const_iterator it = ethernet_services.begin();
         it != ethernet_services.end(); ++it) {
      const NetworkState* ethernet_service = *it;
      if (ethernet_service->update_received() ||
          ethernet_service->device_path() != device->path()) {
        continue;
      }
      RequestUpdateForNetwork(ethernet_service->path());
    }
  }
}

void NetworkStateHandler::UpdateIPConfigProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const std::string& ip_config_path,
    const base::DictionaryValue& properties) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    NetworkState* network = GetModifiableNetworkState(path);
    if (!network)
      return;
    network->IPConfigPropertiesChanged(properties);
    NotifyNetworkPropertiesUpdated(network);
    if (network->path() == default_network_path_)
      NotifyDefaultNetworkChanged(network);
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    DeviceState* device = GetModifiableDeviceState(path);
    if (!device)
      return;
    device->IPConfigPropertiesChanged(ip_config_path, properties);
    NotifyDevicePropertiesUpdated(device);
    if (!default_network_path_.empty()) {
      const NetworkState* default_network =
          GetNetworkState(default_network_path_);
      if (default_network && default_network->device_path() == path)
        NotifyDefaultNetworkChanged(default_network);
    }
  }
}

void NetworkStateHandler::CheckPortalListChanged(
    const std::string& check_portal_list) {
  check_portal_list_ = check_portal_list;
}

void NetworkStateHandler::TechnologyListChanged() {
  // Eventually we would like to replace Technology state with Device state.
  // For now, treat technology state changes as device list changes.
  NotifyDeviceListChanged();
}

void NetworkStateHandler::ManagedStateListChanged(
    ManagedState::ManagedType type) {
  SCOPED_NET_LOG_IF_SLOW();
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    SortNetworkList();
    UpdateNetworkStats();
    NotifyNetworkListChanged();
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    std::string devices;
    for (auto iter = device_list_.begin(); iter != device_list_.end(); ++iter) {
      if (iter != device_list_.begin())
        devices += ", ";
      devices += (*iter)->name();
    }
    NET_LOG_EVENT("DeviceList", devices);
    NotifyDeviceListChanged();
  } else {
    NOTREACHED();
  }
}

// TODO(khorimoto): Add sorting for the Tether network list as well.
void NetworkStateHandler::SortNetworkList() {
  // Note: usually active networks will precede inactive networks, however
  // this may briefly be untrue during state transitions (e.g. a network may
  // transition to idle before the list is updated).
  ManagedStateList active, non_wifi_visible, wifi_visible, hidden, new_networks;
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (!network->update_received()) {
      new_networks.push_back(std::move(*iter));
      continue;
    }
    if (network->IsConnectedState() || network->IsConnectingState()) {
      active.push_back(std::move(*iter));
      continue;
    }
    if (network->visible()) {
      if (NetworkTypePattern::WiFi().MatchesType(network->type()))
        wifi_visible.push_back(std::move(*iter));
      else
        non_wifi_visible.push_back(std::move(*iter));
    } else {
      hidden.push_back(std::move(*iter));
    }
  }
  network_list_.clear();
  network_list_ = std::move(active);
  std::move(non_wifi_visible.begin(), non_wifi_visible.end(),
            std::back_inserter(network_list_));
  std::move(wifi_visible.begin(), wifi_visible.end(),
            std::back_inserter(network_list_));
  std::move(hidden.begin(), hidden.end(), std::back_inserter(network_list_));
  std::move(new_networks.begin(), new_networks.end(),
            std::back_inserter(network_list_));
  network_list_sorted_ = true;
}

void NetworkStateHandler::UpdateNetworkStats() {
  size_t shared = 0, unshared = 0, visible = 0;
  for (ManagedStateList::iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (network->visible())
      ++visible;
    if (network->IsInProfile()) {
      if (network->IsPrivate())
        ++unshared;
      else
        ++shared;
    }
  }
  UMA_HISTOGRAM_COUNTS_100("Networks.Visible", visible);
  UMA_HISTOGRAM_COUNTS_100("Networks.RememberedShared", shared);
  UMA_HISTOGRAM_COUNTS_100("Networks.RememberedUnshared", unshared);
}

void NetworkStateHandler::DefaultNetworkServiceChanged(
    const std::string& service_path) {
  // Shill uses '/' for empty service path values; check explicitly for that.
  const char kEmptyServicePath[] = "/";
  std::string new_service_path =
      (service_path != kEmptyServicePath) ? service_path : "";
  if (new_service_path == default_network_path_)
    return;

  if (new_service_path.empty()) {
    // If Shill reports that there is no longer a default network but there is
    // still an active Tether connection corresponding to the default network,
    // return early without changing |default_network_path_|. Observers will be
    // notified of the default network change due to a subsequent call to
    // SetTetherNetworkStateDisconnected().
    const NetworkState* old_default_network = DefaultNetwork();
    if (old_default_network && old_default_network->type() == kTypeTether)
      return;
  }

  default_network_path_ = service_path;
  NET_LOG_EVENT("DefaultNetworkServiceChanged:", default_network_path_);
  const NetworkState* network = nullptr;
  if (!default_network_path_.empty()) {
    network = GetNetworkState(default_network_path_);
    if (!network) {
      // If NetworkState is not available yet, do not notify observers here,
      // they will be notified when the state is received.
      NET_LOG(EVENT) << "Default NetworkState not available: "
                     << default_network_path_;
      return;
    }
    if (!network->tether_guid().empty()) {
      DCHECK(network->type() == shill::kTypeWifi);

      // If the new default network from Shill's point of view is a Wi-Fi
      // network which corresponds to a hotspot for a Tether network, set the
      // default network to be the associated Tether network instead.
      default_network_path_ = network->tether_guid();
      return;
    }
  }
  if (network && !network->IsConnectedState()) {
    if (network->IsConnectingState()) {
      NET_LOG(EVENT) << "DefaultNetwork is connecting: " << GetLogName(network)
                     << ": " << network->connection_state();
    } else {
      NET_LOG(ERROR) << "DefaultNetwork in unexpected state: "
                     << GetLogName(network) << ": "
                     << network->connection_state();
    }
    // Do not notify observers here, the notification will occur when the
    // connection state changes.
    return;
  }
  NotifyDefaultNetworkChanged(network);
}

//------------------------------------------------------------------------------
// Private methods

void NetworkStateHandler::UpdateGuid(NetworkState* network) {
  std::string specifier = network->GetSpecifier();
  DCHECK(!specifier.empty());
  if (!network->guid().empty()) {
    // If the network is saved in a profile, remove the entry from the map.
    // Otherwise ensure that the entry matches the specified GUID. (e.g. in
    // case a visible network with a specified guid gets configured with a
    // new guid).
    if (network->IsInProfile())
      specifier_guid_map_.erase(specifier);
    else
      specifier_guid_map_[specifier] = network->guid();
    return;
  }
  // Ensure that the NetworkState has a valid GUID.
  std::string guid;
  SpecifierGuidMap::iterator iter = specifier_guid_map_.find(specifier);
  if (iter != specifier_guid_map_.end()) {
    guid = iter->second;
  } else {
    guid = base::GenerateGUID();
    specifier_guid_map_[specifier] = guid;
  }
  network->SetGuid(guid);
}

void NetworkStateHandler::NotifyNetworkListChanged() {
  NET_LOG_EVENT("NOTIFY:NetworkListChanged",
                base::StringPrintf("Size:%" PRIuS, network_list_.size()));
  for (auto& observer : observers_)
    observer.NetworkListChanged();
}

void NetworkStateHandler::NotifyDeviceListChanged() {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:DeviceListChanged",
                base::StringPrintf("Size:%" PRIuS, device_list_.size()));
  for (auto& observer : observers_)
    observer.DeviceListChanged();
}

DeviceState* NetworkStateHandler::GetModifiableDeviceState(
    const std::string& device_path) const {
  ManagedState* managed = GetModifiableManagedState(&device_list_, device_path);
  if (!managed)
    return nullptr;
  return managed->AsDeviceState();
}

NetworkState* NetworkStateHandler::GetModifiableNetworkState(
    const std::string& service_path) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed) {
    managed = GetModifiableManagedState(&tether_network_list_, service_path);
    if (!managed)
      return nullptr;
  }
  return managed->AsNetworkState();
}

NetworkState* NetworkStateHandler::GetModifiableNetworkStateFromGuid(
    const std::string& guid) const {
  for (auto iter = tether_network_list_.begin();
       iter != tether_network_list_.end(); ++iter) {
    NetworkState* tether_network = (*iter)->AsNetworkState();
    if (tether_network->guid() == guid)
      return tether_network;
  }

  for (auto iter = network_list_.begin(); iter != network_list_.end(); ++iter) {
    NetworkState* network = (*iter)->AsNetworkState();
    if (network->guid() == guid)
      return network;
  }

  return nullptr;
}

ManagedState* NetworkStateHandler::GetModifiableManagedState(
    const ManagedStateList* managed_list,
    const std::string& path) const {
  for (auto iter = managed_list->begin(); iter != managed_list->end(); ++iter) {
    ManagedState* managed = iter->get();
    if (managed->path() == path)
      return managed;
  }
  return nullptr;
}

NetworkStateHandler::ManagedStateList* NetworkStateHandler::GetManagedList(
    ManagedState::ManagedType type) {
  switch (type) {
    case ManagedState::MANAGED_TYPE_NETWORK:
      return &network_list_;
    case ManagedState::MANAGED_TYPE_DEVICE:
      return &device_list_;
  }
  NOTREACHED();
  return nullptr;
}

void NetworkStateHandler::OnNetworkConnectionStateChanged(
    NetworkState* network) {
  SCOPED_NET_LOG_IF_SLOW();
  DCHECK(network);
  bool notify_default = false;
  if (network->path() == default_network_path_) {
    if (network->IsConnectedState()) {
      notify_default = true;
    } else if (network->IsConnectingState()) {
      // Wait until the network is actually connected to notify that the default
      // network changed.
      NET_LOG(EVENT) << "Default network is not connected: "
                     << GetLogName(network)
                     << "State: " << network->connection_state();
    } else {
      NET_LOG(ERROR) << "Default network in unexpected state: "
                     << GetLogName(network)
                     << "State: " << network->connection_state();
      default_network_path_.clear();
      SortNetworkList();
      NotifyDefaultNetworkChanged(nullptr);
    }
  }
  std::string desc = "NetworkConnectionStateChanged";
  if (notify_default)
    desc = "Default" + desc;
  NET_LOG(EVENT) << "NOTIFY: " << desc << ": " << GetLogName(network) << ": "
                 << network->connection_state();
  for (auto& observer : observers_)
    observer.NetworkConnectionStateChanged(network);
  if (notify_default)
    NotifyDefaultNetworkChanged(network);
}

void NetworkStateHandler::NotifyDefaultNetworkChanged(
    const NetworkState* default_network) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_EVENT("NOTIFY:DefaultNetworkChanged", GetLogName(default_network));
  for (auto& observer : observers_)
    observer.DefaultNetworkChanged(default_network);
}

void NetworkStateHandler::NotifyNetworkPropertiesUpdated(
    const NetworkState* network) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:NetworkPropertiesUpdated", GetLogName(network));
  for (auto& observer : observers_)
    observer.NetworkPropertiesUpdated(network);
}

void NetworkStateHandler::NotifyDevicePropertiesUpdated(
    const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:DevicePropertiesUpdated", GetLogName(device));
  for (auto& observer : observers_)
    observer.DevicePropertiesUpdated(device);
}

void NetworkStateHandler::NotifyScanRequested() {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:ScanRequested", "");
  for (auto& observer : observers_)
    observer.ScanRequested();
}

void NetworkStateHandler::NotifyScanCompleted(const DeviceState* device) {
  SCOPED_NET_LOG_IF_SLOW();
  NET_LOG_DEBUG("NOTIFY:ScanCompleted", GetLogName(device));
  for (auto& observer : observers_)
    observer.ScanCompleted(device);
}

std::string NetworkStateHandler::GetTechnologyForType(
    const NetworkTypePattern& type) const {
  if (type.MatchesType(shill::kTypeEthernet))
    return shill::kTypeEthernet;

  if (type.MatchesType(shill::kTypeWifi))
    return shill::kTypeWifi;

  if (type.Equals(NetworkTypePattern::Wimax()))
    return shill::kTypeWimax;

  // Prefer Wimax over Cellular only if it's available.
  if (type.MatchesType(shill::kTypeWimax) &&
      shill_property_handler_->IsTechnologyAvailable(shill::kTypeWimax)) {
    return shill::kTypeWimax;
  }

  if (type.MatchesType(shill::kTypeCellular))
    return shill::kTypeCellular;

  if (type.MatchesType(kTypeTether))
    return kTypeTether;

  NOTREACHED();
  return std::string();
}

std::vector<std::string> NetworkStateHandler::GetTechnologiesForType(
    const NetworkTypePattern& type) const {
  std::vector<std::string> technologies;
  if (type.MatchesType(shill::kTypeEthernet))
    technologies.emplace_back(shill::kTypeEthernet);
  if (type.MatchesType(shill::kTypeWifi))
    technologies.emplace_back(shill::kTypeWifi);
  if (type.MatchesType(shill::kTypeWimax))
    technologies.emplace_back(shill::kTypeWimax);
  if (type.MatchesType(shill::kTypeCellular))
    technologies.emplace_back(shill::kTypeCellular);
  if (type.MatchesType(shill::kTypeBluetooth))
    technologies.emplace_back(shill::kTypeBluetooth);
  if (type.MatchesType(shill::kTypeVPN))
    technologies.emplace_back(shill::kTypeVPN);
  if (type.MatchesType(kTypeTether))
    technologies.emplace_back(kTypeTether);

  CHECK_GT(technologies.size(), 0ul);
  return technologies;
}

}  // namespace chromeos
