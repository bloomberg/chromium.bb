// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_state_handler.h"

#include "base/format_macros.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_state.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/shill_property_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {

const char kLogModule[] = "NetworkStateHandler";

// Returns true if |network->type()| == |match_type|, or it matches one of the
// following special match types:
// * kMatchTypeDefault matches any network (i.e. the first instance)
// * kMatchTypeNonVirtual matches non virtual networks
// * kMatchTypeWireless matches wireless networks
// * kMatchTypeMobile matches cellular or wimax networks
bool ManagedStateMatchesType(const chromeos::ManagedState* managed,
                             const std::string& match_type) {
  const std::string& type = managed->type();
  if (match_type == chromeos::NetworkStateHandler::kMatchTypeDefault)
    return true;
  if (match_type == type)
    return true;
  if (match_type == chromeos::NetworkStateHandler::kMatchTypeNonVirtual &&
      type != flimflam::kTypeVPN) {
    return true;
  }
  if (match_type == chromeos::NetworkStateHandler::kMatchTypeWireless &&
      type != flimflam::kTypeEthernet && type != flimflam::kTypeVPN) {
    return true;
  }
  if (match_type == chromeos::NetworkStateHandler::kMatchTypeMobile &&
      (type == flimflam::kTypeCellular || type == flimflam::kTypeWimax)) {
    return true;
  }
  return false;
}

std::string ValueAsString(const base::Value& value) {
  if (value.GetType() == base::Value::TYPE_BOOLEAN) {
    bool bval = false;
    value.GetAsBoolean(&bval);
    return bval ? "true" : "false";
  } else if (value.GetType() == base::Value::TYPE_INTEGER) {
    int intval = 0;
    value.GetAsInteger(&intval);
    return base::StringPrintf("%d", intval);
  } else if (value.GetType() == base::Value::TYPE_DOUBLE) {
    double dval = 0;
    value.GetAsDouble(&dval);
    return base::StringPrintf("%g", dval);
  } else if (value.GetType() == base::Value::TYPE_STRING) {
    std::string vstr;
    value.GetAsString(&vstr);
    return vstr;
  }
  return "";
}

}  // namespace

namespace chromeos {

const char NetworkStateHandler::kMatchTypeDefault[] = "default";
const char NetworkStateHandler::kMatchTypeWireless[] = "wireless";
const char NetworkStateHandler::kMatchTypeMobile[] = "mobile";
const char NetworkStateHandler::kMatchTypeNonVirtual[] = "non-virtual";

static NetworkStateHandler* g_network_state_handler = NULL;

NetworkStateHandler::NetworkStateHandler() {
}

NetworkStateHandler::~NetworkStateHandler() {
  STLDeleteContainerPointers(network_list_.begin(), network_list_.end());
  STLDeleteContainerPointers(device_list_.begin(), device_list_.end());
}

void NetworkStateHandler::InitShillPropertyHandler() {
  shill_property_handler_.reset(new internal::ShillPropertyHandler(this));
  shill_property_handler_->Init();
}

// static
void NetworkStateHandler::Initialize() {
  CHECK(!g_network_state_handler);
  g_network_state_handler = new NetworkStateHandler();
  g_network_state_handler->InitShillPropertyHandler();
}

// static
bool NetworkStateHandler::IsInitialized() {
  return g_network_state_handler != NULL;
}

// static
void NetworkStateHandler::Shutdown() {
  CHECK(g_network_state_handler);
  delete g_network_state_handler;
  g_network_state_handler = NULL;
}

// static
NetworkStateHandler* NetworkStateHandler::Get() {
  CHECK(g_network_state_handler)
      << "NetworkStateHandler::Get() called before Initialize()";
  return g_network_state_handler;
}

void NetworkStateHandler::AddObserver(NetworkStateHandlerObserver* observer) {
  observers_.AddObserver(observer);
}

void NetworkStateHandler::RemoveObserver(
    NetworkStateHandlerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool NetworkStateHandler::TechnologyAvailable(const std::string& type) const {
  if (type == kMatchTypeMobile) {
    return shill_property_handler_->TechnologyAvailable(flimflam::kTypeWimax) ||
        shill_property_handler_->TechnologyAvailable(flimflam::kTypeCellular);
  }
  return shill_property_handler_->TechnologyAvailable(type);
}

bool NetworkStateHandler::TechnologyEnabled(const std::string& type) const {
  if (type == kMatchTypeMobile) {
    return shill_property_handler_->TechnologyEnabled(flimflam::kTypeWimax) ||
        shill_property_handler_->TechnologyEnabled(flimflam::kTypeCellular);
  }
  return shill_property_handler_->TechnologyEnabled(type);
}

bool NetworkStateHandler::TechnologyUninitialized(
    const std::string& type) const {
  if (type == kMatchTypeMobile) {
    return
        shill_property_handler_->TechnologyUninitialized(
            flimflam::kTypeWimax) ||
        shill_property_handler_->TechnologyUninitialized(
            flimflam::kTypeCellular);
  }
  return shill_property_handler_->TechnologyUninitialized(type);
}


void NetworkStateHandler::SetTechnologyEnabled(
    const std::string& type,
    bool enabled,
    const network_handler::ErrorCallback& error_callback) {
  if (type == kMatchTypeMobile) {
    shill_property_handler_->SetTechnologyEnabled(
        flimflam::kTypeCellular, enabled, error_callback);
    shill_property_handler_->SetTechnologyEnabled(
        flimflam::kTypeWimax, enabled, error_callback);
  } else {
    shill_property_handler_->SetTechnologyEnabled(
        type, enabled, error_callback);
  }
}

const DeviceState* NetworkStateHandler::GetDeviceState(
    const std::string& device_path) const {
  return GetModifiableDeviceState(device_path);
}

const DeviceState* NetworkStateHandler::GetDeviceStateByType(
    const std::string& type) const {
  for (ManagedStateList::const_iterator iter = device_list_.begin();
       iter != device_list_.end(); ++iter) {
    ManagedState* device = *iter;
    if (ManagedStateMatchesType(device, type))
      return device->AsDeviceState();
  }
  return NULL;
}

bool NetworkStateHandler::GetScanningByType(const std::string& type) const {
  for (ManagedStateList::const_iterator iter = device_list_.begin();
       iter != device_list_.end(); ++iter) {
    const DeviceState* device = (*iter)->AsDeviceState();
    DCHECK(device);
    if (ManagedStateMatchesType(device, type) && device->scanning())
      return true;
  }
  return false;
}

const NetworkState* NetworkStateHandler::GetNetworkState(
    const std::string& service_path) const {
  return GetModifiableNetworkState(service_path);
}

const NetworkState* NetworkStateHandler::DefaultNetwork() const {
  if (network_list_.empty())
    return NULL;
  const NetworkState* network = network_list_.front()->AsNetworkState();
  DCHECK(network);
  if (!network->IsConnectedState())
    return NULL;
  return network;
}

const NetworkState* NetworkStateHandler::ConnectedNetworkByType(
    const std::string& type) const {
  for (ManagedStateList::const_iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (!network->IsConnectedState())
      break;  // Connected networks are listed first.
    if (ManagedStateMatchesType(network, type))
      return network;
  }
  return NULL;
}

const NetworkState* NetworkStateHandler::ConnectingNetworkByType(
    const std::string& type) const {
  for (ManagedStateList::const_iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (network->IsConnectedState())
      continue;
    if (!network->IsConnectingState())
      break;  // Connected and connecting networks are listed first.
    if (ManagedStateMatchesType(network, type))
      return network;
  }
  return NULL;
}

const NetworkState* NetworkStateHandler::FirstNetworkByType(
    const std::string& type) const {
  for (ManagedStateList::const_iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    if (ManagedStateMatchesType(network, type))
      return network;
  }
  return NULL;
}

std::string NetworkStateHandler::HardwareAddressForType(
    const std::string& type) const {
  std::string result;
  const NetworkState* network = ConnectedNetworkByType(type);
  if (network) {
    const DeviceState* device = GetDeviceState(network->device_path());
    if (device)
      result = device->mac_address();
  }
  StringToUpperASCII(&result);
  return result;
}

std::string NetworkStateHandler::FormattedHardwareAddressForType(
    const std::string& type) const {
  std::string address = HardwareAddressForType(type);
  if (address.size() % 2 != 0)
    return address;
  std::string result;
  for (size_t i = 0; i < address.size(); ++i) {
    if ((i != 0) && (i % 2 == 0))
      result.push_back(':');
    result.push_back(address[i]);
  }
  return result;
}

void NetworkStateHandler::GetNetworkList(NetworkStateList* list) const {
  DCHECK(list);
  NetworkStateList result;
  list->clear();
  for (ManagedStateList::const_iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    const NetworkState* network = (*iter)->AsNetworkState();
    DCHECK(network);
    list->push_back(network);
  }
}

void NetworkStateHandler::RequestScan() const {
  shill_property_handler_->RequestScan();
}

void NetworkStateHandler::SetConnectingNetwork(
    const std::string& service_path) {
  connecting_network_ = service_path;
  network_event_log::AddEntry(
      kLogModule, "SetConnectingNetwork", service_path);
}

void NetworkStateHandler::GetNetworkStatePropertiesForTest(
    base::DictionaryValue* dictionary) const {
  for (ManagedStateList::const_iterator iter = network_list_.begin();
       iter != network_list_.end(); ++iter) {
    base::DictionaryValue* network_dict = new base::DictionaryValue;
    (*iter)->AsNetworkState()->GetProperties(network_dict);
    dictionary->SetWithoutPathExpansion((*iter)->path(), network_dict);
  }
}

//------------------------------------------------------------------------------
// ShillPropertyHandler::Delegate overrides

void NetworkStateHandler::UpdateManagedList(ManagedState::ManagedType type,
                                            const base::ListValue& entries) {
  ManagedStateList* managed_list = GetManagedList(type);
  VLOG(2) << "UpdateManagedList: " << type;
  // Create a map of existing entries.
  std::map<std::string, ManagedState*> managed_map;
  for (ManagedStateList::iterator iter = managed_list->begin();
       iter != managed_list->end(); ++iter) {
    ManagedState* managed = *iter;
    managed_map[managed->path()] = managed;
  }
  // Clear the list (pointers are owned by managed_map).
  managed_list->clear();
  // Updates managed_list and request updates for new entries.
  for (base::ListValue::const_iterator iter = entries.begin();
       iter != entries.end(); ++iter) {
    std::string path;
    (*iter)->GetAsString(&path);
    DCHECK(!path.empty());
    std::map<std::string, ManagedState*>::iterator found =
        managed_map.find(path);
    bool request_properties = false;
    ManagedState* managed;
    bool is_observing = shill_property_handler_->IsObservingNetwork(path);
    if (found == managed_map.end()) {
      request_properties = true;
      managed = ManagedState::Create(type, path);
      managed_list->push_back(managed);
    } else {
      managed = found->second;
      managed_list->push_back(managed);
      managed_map.erase(found);
      if (!managed->is_observed() && is_observing)
        request_properties = true;
    }
    if (is_observing)
      managed->set_is_observed(true);
    if (request_properties)
      shill_property_handler_->RequestProperties(type, path);
  }
  // Delete any remaning entries in managed_map.
  STLDeleteContainerPairSecondPointers(managed_map.begin(), managed_map.end());
}

void NetworkStateHandler::UpdateManagedStateProperties(
    ManagedState::ManagedType type,
    const std::string& path,
    const base::DictionaryValue& properties) {
  ManagedState* managed = GetModifiableManagedState(GetManagedList(type), path);
  if (!managed) {
    LOG(ERROR) << "GetPropertiesCallback: " << path << " Not found!";
    return;
  }
  bool network_property_updated = false;
  std::string prev_connection_state;
  if (type == ManagedState::MANAGED_TYPE_NETWORK)
    prev_connection_state = managed->AsNetworkState()->connection_state();
  for (base::DictionaryValue::Iterator iter(properties);
       iter.HasNext(); iter.Advance()) {
    if (type == ManagedState::MANAGED_TYPE_NETWORK) {
      if (managed->PropertyChanged(iter.key(), iter.value()))
        network_property_updated = true;
    } else {
      managed->PropertyChanged(iter.key(), iter.value());
    }
  }
  // Notify observers.
  if (network_property_updated) {
    NetworkState* network = managed->AsNetworkState();
    DCHECK(network);
    // Signal connection state changed after all properties have been updated.
    if (network->connection_state() != prev_connection_state)
      OnNetworkConnectionStateChanged(network);
    NetworkPropertiesUpdated(network);
  }
  network_event_log::AddEntry(
      kLogModule, "PropertiesReceived",
      base::StringPrintf("%s (%s)", path.c_str(), managed->name().c_str()));
}

void NetworkStateHandler::UpdateNetworkServiceProperty(
    const std::string& service_path,
    const std::string& key,
    const base::Value& value) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network)
    return;
  std::string prev_connection_state = network->connection_state();
  if (!network->PropertyChanged(key, value))
    return;
  if (network->connection_state() != prev_connection_state)
    OnNetworkConnectionStateChanged(network);

  NetworkPropertiesUpdated(network);

  std::string detail = network->name() + "." + key;
  std::string vstr = ValueAsString(value);
  if (!vstr.empty())
    detail += " = " + vstr;
  network_event_log::AddEntry(kLogModule, "NetworkPropertyUpdated", detail);
}

void NetworkStateHandler::UpdateNetworkServiceIPAddress(
    const std::string& service_path,
    const std::string& ip_address) {
  NetworkState* network = GetModifiableNetworkState(service_path);
  if (!network)
    return;
  std::string detail = network->name() + ".IPAddress = " + ip_address;
  network_event_log::AddEntry(kLogModule, "NetworkIPChanged", detail);
  network->set_ip_address(ip_address);
  NetworkPropertiesUpdated(network);
}

void NetworkStateHandler::UpdateDeviceProperty(const std::string& device_path,
                                               const std::string& key,
                                               const base::Value& value) {
  DeviceState* device = GetModifiableDeviceState(device_path);
  if (!device)
    return;
  if (!device->PropertyChanged(key, value))
    return;

  FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                    DeviceListChanged());

  std::string detail = device->name() + "." + key;
  std::string vstr = ValueAsString(value);
  if (!vstr.empty())
    detail += " = " + vstr;
  network_event_log::AddEntry(kLogModule, "DevicePropertyUpdated", detail);
}

void NetworkStateHandler::ManagerPropertyChanged() {
  FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                    NetworkManagerChanged());
}

void NetworkStateHandler::ManagedStateListChanged(
    ManagedState::ManagedType type) {
  if (type == ManagedState::MANAGED_TYPE_NETWORK) {
    // Notify observers that the list of networks has changed.
    network_event_log::AddEntry(
        kLogModule, "NetworkListChanged",
        base::StringPrintf("Size: %"PRIuS, network_list_.size()));
    FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                      NetworkListChanged());
    // The list order may have changed, so check if the default network changed.
    if (CheckDefaultNetworkChanged())
      OnDefaultNetworkChanged();
  } else if (type == ManagedState::MANAGED_TYPE_DEVICE) {
    network_event_log::AddEntry(
        kLogModule, "DeviceListChanged",
        base::StringPrintf("Size: %"PRIuS, device_list_.size()));
    FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                      DeviceListChanged());
  } else {
    NOTREACHED();
  }
}

//------------------------------------------------------------------------------
// Private methods

DeviceState* NetworkStateHandler::GetModifiableDeviceState(
    const std::string& device_path) const {
  ManagedState* managed = GetModifiableManagedState(&device_list_, device_path);
  if (!managed)
    return NULL;
  return managed->AsDeviceState();
}

NetworkState* NetworkStateHandler::GetModifiableNetworkState(
    const std::string& service_path) const {
  ManagedState* managed =
      GetModifiableManagedState(&network_list_, service_path);
  if (!managed)
    return NULL;
  return managed->AsNetworkState();
}

ManagedState* NetworkStateHandler::GetModifiableManagedState(
    const ManagedStateList* managed_list,
    const std::string& path) const {
  for (ManagedStateList::const_iterator iter = managed_list->begin();
       iter != managed_list->end(); ++iter) {
    ManagedState* managed = *iter;
    if (managed->path() == path)
      return managed;
  }
  return NULL;
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
  return NULL;
}

void NetworkStateHandler::OnNetworkConnectionStateChanged(
    NetworkState* network) {
  DCHECK(network);
  std::string desc = base::StringPrintf(
      "%s: %s", network->path().c_str(), network->connection_state().c_str());
  network_event_log::AddEntry(
      kLogModule, "NetworkConnectionStateChanged", desc);
  FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                    NetworkConnectionStateChanged(network));
  if (CheckDefaultNetworkChanged() || network->path() == default_network_path_)
    OnDefaultNetworkChanged();
}

bool NetworkStateHandler::CheckDefaultNetworkChanged() {
  std::string new_default_network_path;
  const NetworkState* new_default_network = DefaultNetwork();
  if (new_default_network)
    new_default_network_path = new_default_network->path();
  if (new_default_network_path == default_network_path_)
    return false;
  default_network_path_ = new_default_network_path;
  return true;
}

void NetworkStateHandler::OnDefaultNetworkChanged() {
  const NetworkState* default_network = DefaultNetwork();
  network_event_log::AddEntry(
      kLogModule, "DefaultNetworkChanged",
      default_network ? default_network->path() : "None");
  FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                    DefaultNetworkChanged(default_network));
}

void NetworkStateHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  FOR_EACH_OBSERVER(NetworkStateHandlerObserver, observers_,
                    NetworkPropertiesUpdated(network));
  // If |connecting_network_| transitions to a non-idle, non-connecting state,
  // clear it *after* signalling observers.
  if (network->path() == connecting_network_ &&
      !network->IsConnectingState() &&
      network->connection_state() != flimflam::kStateIdle) {
    connecting_network_.clear();
    network_event_log::AddEntry(
        kLogModule, "ClearConnectingNetwork",
        base::StringPrintf("%s: %s", network->path().c_str(),
                           network->connection_state().c_str()));
  }
}

}  // namespace chromeos
