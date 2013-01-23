// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library_impl_base.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chrome/browser/chromeos/network_login_observer.h"
#include "chromeos/network/onc/onc_certificate_importer.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_normalizer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
#include "chromeos/network/onc/onc_validator.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"  // crypto::GetTPMTokenInfo() for 802.1X and VPN.
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Only send network change notifications to observers once every 50ms.
const int kNetworkNotifyDelayMs = 50;

// How long we should remember that cellular plan payment was received.
const int kRecentPlanPaymentHours = 6;

NetworkProfileType GetProfileTypeForSource(onc::ONCSource source) {
  switch (source) {
    case onc::ONC_SOURCE_DEVICE_POLICY:
      return PROFILE_SHARED;
    case onc::ONC_SOURCE_USER_POLICY:
      return PROFILE_USER;
    case onc::ONC_SOURCE_NONE:
    case onc::ONC_SOURCE_USER_IMPORT:
      return PROFILE_NONE;
  }
  NOTREACHED() << "Unknown ONC source " << source;
  return PROFILE_NONE;
}

}  // namespace

NetworkLibraryImplBase::NetworkLibraryImplBase()
    : ethernet_(NULL),
      active_wifi_(NULL),
      active_cellular_(NULL),
      active_wimax_(NULL),
      active_virtual_(NULL),
      available_devices_(0),
      uninitialized_devices_(0),
      enabled_devices_(0),
      busy_devices_(0),
      wifi_scanning_(false),
      offline_mode_(false),
      is_locked_(false),
      sim_operation_(SIM_OPERATION_NONE),
      ALLOW_THIS_IN_INITIALIZER_LIST(notify_manager_weak_factory_(this)) {
  network_login_observer_.reset(new NetworkLoginObserver());
  AddNetworkManagerObserver(network_login_observer_.get());
}

NetworkLibraryImplBase::~NetworkLibraryImplBase() {
  network_profile_observers_.Clear();
  network_manager_observers_.Clear();
  pin_operation_observers_.Clear();
  user_action_observers_.Clear();
  STLDeleteValues(&network_map_);
  ClearNetworks();
  DeleteRememberedNetworks();
  STLDeleteValues(&device_map_);
  STLDeleteValues(&network_device_observers_);
  STLDeleteValues(&network_observers_);
  STLDeleteValues(&network_onc_map_);
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplBase::AddNetworkProfileObserver(
    NetworkProfileObserver* observer) {
  network_profile_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveNetworkProfileObserver(
    NetworkProfileObserver* observer) {
  network_profile_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddNetworkManagerObserver(
    NetworkManagerObserver* observer) {
  if (!network_manager_observers_.HasObserver(observer))
    network_manager_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveNetworkManagerObserver(
    NetworkManagerObserver* observer) {
  network_manager_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddNetworkObserver(
    const std::string& service_path, NetworkObserver* observer) {
  // First, add the observer to the callback map.
  NetworkObserverMap::iterator iter = network_observers_.find(service_path);
  NetworkObserverList* oblist;
  if (iter != network_observers_.end()) {
    oblist = iter->second;
  } else {
    oblist = new NetworkObserverList();
    network_observers_[service_path] = oblist;
  }
  if (observer && !oblist->HasObserver(observer))
    oblist->AddObserver(observer);
  MonitorNetworkStart(service_path);
}

void NetworkLibraryImplBase::RemoveNetworkObserver(
    const std::string& service_path, NetworkObserver* observer) {
  DCHECK(service_path.size());
  NetworkObserverMap::iterator map_iter =
      network_observers_.find(service_path);
  if (map_iter != network_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
    if (!map_iter->second->size()) {
      MonitorNetworkStop(service_path);
      delete map_iter->second;
      network_observers_.erase(map_iter);
    }
  }
}

void NetworkLibraryImplBase::RemoveObserverForAllNetworks(
    NetworkObserver* observer) {
  DCHECK(observer);
  NetworkObserverMap::iterator map_iter = network_observers_.begin();
  while (map_iter != network_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
    if (!map_iter->second->size()) {
      MonitorNetworkStop(map_iter->first);
      delete map_iter->second;
      network_observers_.erase(map_iter++);
    } else {
      ++map_iter;
    }
  }
}

void NetworkLibraryImplBase::AddNetworkDeviceObserver(
    const std::string& device_path, NetworkDeviceObserver* observer) {
  // First, add the observer to the callback map.
  NetworkDeviceObserverMap::iterator iter =
      network_device_observers_.find(device_path);
  NetworkDeviceObserverList* oblist;
  if (iter != network_device_observers_.end()) {
    oblist = iter->second;
  } else {
    oblist = new NetworkDeviceObserverList();
    network_device_observers_[device_path] = oblist;
  }
  if (!oblist->HasObserver(observer))
    oblist->AddObserver(observer);
  MonitorNetworkDeviceStart(device_path);
}

void NetworkLibraryImplBase::RemoveNetworkDeviceObserver(
    const std::string& device_path, NetworkDeviceObserver* observer) {
  DCHECK(device_path.size());
  NetworkDeviceObserverMap::iterator map_iter =
      network_device_observers_.find(device_path);
  if (map_iter != network_device_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
  }
}

void NetworkLibraryImplBase::DeleteDeviceFromDeviceObserversMap(
    const std::string& device_path) {
  // Delete all device observers associated with this device.
  NetworkDeviceObserverMap::iterator map_iter =
      network_device_observers_.find(device_path);
  if (map_iter != network_device_observers_.end()) {
    delete map_iter->second;
    network_device_observers_.erase(map_iter);
  }
}

//////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::Lock() {
  if (is_locked_)
    return;
  is_locked_ = true;
  NotifyNetworkManagerChanged(true);  // Forced update.
}

void NetworkLibraryImplBase::Unlock() {
  DCHECK(is_locked_);
  if (!is_locked_)
    return;
  is_locked_ = false;
  NotifyNetworkManagerChanged(true);  // Forced update.
}

bool NetworkLibraryImplBase::IsLocked() {
  return is_locked_;
}

//////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::AddPinOperationObserver(
    PinOperationObserver* observer) {
  if (!pin_operation_observers_.HasObserver(observer))
    pin_operation_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemovePinOperationObserver(
    PinOperationObserver* observer) {
  pin_operation_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddUserActionObserver(
    UserActionObserver* observer) {
  if (!user_action_observers_.HasObserver(observer))
    user_action_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveUserActionObserver(
    UserActionObserver* observer) {
  user_action_observers_.RemoveObserver(observer);
}

const EthernetNetwork* NetworkLibraryImplBase::ethernet_network() const {
  return ethernet_;
}

bool NetworkLibraryImplBase::ethernet_connecting() const {
  return ethernet_ ? ethernet_->connecting() : false;
}
bool NetworkLibraryImplBase::ethernet_connected() const {
  return ethernet_ ? ethernet_->connected() : false;
}
const WifiNetwork* NetworkLibraryImplBase::wifi_network() const {
  return active_wifi_;
}
bool NetworkLibraryImplBase::wifi_connecting() const {
  return active_wifi_ ? active_wifi_->connecting() : false;
}
bool NetworkLibraryImplBase::wifi_connected() const {
  return active_wifi_ ? active_wifi_->connected() : false;
}
const CellularNetwork* NetworkLibraryImplBase::cellular_network() const {
  return active_cellular_;
}
bool NetworkLibraryImplBase::cellular_connecting() const {
  return active_cellular_ ? active_cellular_->connecting() : false;
}
bool NetworkLibraryImplBase::cellular_connected() const {
  return active_cellular_ ? active_cellular_->connected() : false;
}
const WimaxNetwork* NetworkLibraryImplBase::wimax_network() const {
  return active_wimax_;
}
bool NetworkLibraryImplBase::wimax_connecting() const {
  return active_wimax_ ? active_wimax_->connecting() : false;
}
bool NetworkLibraryImplBase::wimax_connected() const {
  return active_wimax_ ? active_wimax_->connected() : false;
}
const Network* NetworkLibraryImplBase::mobile_network() const {
  return active_cellular_ ?
      static_cast<Network*>(active_cellular_) :
      static_cast<Network*>(active_wimax_);
}
bool NetworkLibraryImplBase::mobile_connecting() const {
  return cellular_connecting() || wimax_connecting();
}
bool NetworkLibraryImplBase::mobile_connected() const {
  return wimax_connecting() || wimax_connected();
}
const VirtualNetwork* NetworkLibraryImplBase::virtual_network() const {
  return active_virtual_;
}
bool NetworkLibraryImplBase::virtual_network_connecting() const {
  return active_virtual_ ? active_virtual_->connecting() : false;
}
bool NetworkLibraryImplBase::virtual_network_connected() const {
  return active_virtual_ ? active_virtual_->connected() : false;
}
bool NetworkLibraryImplBase::Connected() const {
  return ethernet_connected() || wifi_connected() ||
      cellular_connected() || wimax_connected();
}
bool NetworkLibraryImplBase::Connecting() const {
  return ethernet_connecting() || wifi_connecting() ||
      cellular_connecting() || wimax_connecting();
}
const WifiNetworkVector& NetworkLibraryImplBase::wifi_networks() const {
  return wifi_networks_;
}
const WifiNetworkVector&
    NetworkLibraryImplBase::remembered_wifi_networks() const {
  return remembered_wifi_networks_;
}
const CellularNetworkVector& NetworkLibraryImplBase::cellular_networks() const {
  return cellular_networks_;
}
const WimaxNetworkVector& NetworkLibraryImplBase::wimax_networks() const {
  return wimax_networks_;
}
const VirtualNetworkVector& NetworkLibraryImplBase::virtual_networks() const {
  return virtual_networks_;
}
const VirtualNetworkVector&
    NetworkLibraryImplBase::remembered_virtual_networks() const {
  return remembered_virtual_networks_;
}

// Use shill's ordering of the services to determine which type of
// network to return (i.e. don't assume priority of network types).
// Note: This does not include any virtual networks.
namespace {
const Network* highest_priority(const Network* a, const Network*b) {
  if (!a)
    return b;
  if (!b)
    return a;
  if (b->priority_order() < a->priority_order())
    return b;
  return a;
}
}

const Network* NetworkLibraryImplBase::active_network() const {
  const Network* result = active_nonvirtual_network();
  if (active_virtual_ && active_virtual_->is_active())
    result = highest_priority(result, active_virtual_);
  return result;
}

const Network* NetworkLibraryImplBase::active_nonvirtual_network() const {
  const Network* result = NULL;
  if (ethernet_ && ethernet_->is_active())
    result = ethernet_;
  if (active_wifi_ && active_wifi_->is_active())
    result = highest_priority(result, active_wifi_);
  if (active_cellular_ && active_cellular_->is_active())
    result = highest_priority(result, active_cellular_);
  if (active_wimax_ && active_wimax_->is_active())
    result = highest_priority(result, active_wimax_);
  return result;
}

const Network* NetworkLibraryImplBase::connected_network() const {
  const Network* result = NULL;
  if (ethernet_ && ethernet_->connected())
    result = ethernet_;
  if (active_wifi_ && active_wifi_->connected())
    result = highest_priority(result, active_wifi_);
  if (active_cellular_ && active_cellular_->connected())
    result = highest_priority(result, active_cellular_);
  if (active_wimax_ && active_wimax_->connected())
    result = highest_priority(result, active_wimax_);
  return result;
}

// Connecting order in logical preference.
const Network* NetworkLibraryImplBase::connecting_network() const {
  if (ethernet_connecting())
    return ethernet_network();
  else if (wifi_connecting())
    return wifi_network();
  else if (cellular_connecting())
    return cellular_network();
  else if (wimax_connecting())
    return wimax_network();
  return NULL;
}

bool NetworkLibraryImplBase::ethernet_available() const {
  return available_devices_ & (1 << TYPE_ETHERNET);
}

bool NetworkLibraryImplBase::wifi_available() const {
  return available_devices_ & (1 << TYPE_WIFI);
}

bool NetworkLibraryImplBase::wimax_available() const {
  return available_devices_ & (1 << TYPE_WIMAX);
}

bool NetworkLibraryImplBase::cellular_available() const {
  return available_devices_ & (1 << TYPE_CELLULAR);
}

bool NetworkLibraryImplBase::mobile_available() const {
  return cellular_available() || wimax_available();
}

bool NetworkLibraryImplBase::ethernet_enabled() const {
  return enabled_devices_ & (1 << TYPE_ETHERNET);
}

bool NetworkLibraryImplBase::wifi_enabled() const {
  return enabled_devices_ & (1 << TYPE_WIFI);
}

bool NetworkLibraryImplBase::wimax_enabled() const {
  return enabled_devices_ & (1 << TYPE_WIMAX);
}

bool NetworkLibraryImplBase::cellular_enabled() const {
  return enabled_devices_ & (1 << TYPE_CELLULAR);
}

bool NetworkLibraryImplBase::mobile_enabled() const {
  return cellular_enabled() || wimax_enabled();
}

bool NetworkLibraryImplBase::ethernet_busy() const {
  return busy_devices_ & (1 << TYPE_ETHERNET);
}

bool NetworkLibraryImplBase::wifi_busy() const {
  return busy_devices_ & (1 << TYPE_WIFI);
}

bool NetworkLibraryImplBase::wimax_busy() const {
  return busy_devices_ & (1 << TYPE_WIMAX);
}

bool NetworkLibraryImplBase::cellular_busy() const {
  return busy_devices_ & (1 << TYPE_CELLULAR);
}

bool NetworkLibraryImplBase::mobile_busy() const {
  return cellular_busy() || wimax_busy();
}

bool NetworkLibraryImplBase::wifi_scanning() const {
  return wifi_scanning_;
}

bool NetworkLibraryImplBase::cellular_initializing() const {
  if (uninitialized_devices_ & (1 << TYPE_CELLULAR))
    return true;
  const NetworkDevice* device = FindDeviceByType(TYPE_CELLULAR);
  if (device && device->scanning())
    return true;
  return false;
}

bool NetworkLibraryImplBase::offline_mode() const { return offline_mode_; }

std::string NetworkLibraryImplBase::GetCheckPortalList() const {
  return check_portal_list_;
}

// Returns the IP address for the active network.
// TODO(stevenjb): Fix this for VPNs. See chromium-os:13972.
const std::string& NetworkLibraryImplBase::IPAddress() const {
  const Network* result = active_network();
  if (!result)
    result = connected_network();  // happens if we are connected to a VPN.
  if (!result)
    result = ethernet_;  // Use non active ethernet addr if no active network.
  if (result)
    return result->ip_address();
  CR_DEFINE_STATIC_LOCAL(std::string, null_address, ("0.0.0.0"));
  return null_address;
}

/////////////////////////////////////////////////////////////////////////////

const NetworkDevice* NetworkLibraryImplBase::FindNetworkDeviceByPath(
    const std::string& path) const {
  NetworkDeviceMap::const_iterator iter = device_map_.find(path);
  if (iter != device_map_.end())
    return iter->second;
  LOG(WARNING) << "Device path not found: " << path;
  return NULL;
}

NetworkDevice* NetworkLibraryImplBase::FindNetworkDeviceByPath(
    const std::string& path) {
  NetworkDeviceMap::iterator iter = device_map_.find(path);
  if (iter != device_map_.end())
    return iter->second;
  LOG(WARNING) << "Device path not found: " << path;
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindCellularDevice() const {
  return FindDeviceByType(TYPE_CELLULAR);
}

const NetworkDevice* NetworkLibraryImplBase::FindEthernetDevice() const {
  return FindDeviceByType(TYPE_ETHERNET);
}

const NetworkDevice* NetworkLibraryImplBase::FindWifiDevice() const {
  return FindDeviceByType(TYPE_WIFI);
}

const NetworkDevice* NetworkLibraryImplBase::FindWimaxDevice() const {
  return FindDeviceByType(TYPE_WIMAX);
}

const NetworkDevice* NetworkLibraryImplBase::FindMobileDevice() const {
  const NetworkDevice* device = FindDeviceByType(TYPE_CELLULAR);
  if (device)
    return device;

  return FindDeviceByType(TYPE_WIMAX);
}

Network* NetworkLibraryImplBase::FindNetworkByPath(
    const std::string& path) const {
  NetworkMap::const_iterator iter = network_map_.find(path);
  if (iter != network_map_.end())
    return iter->second;
  return NULL;
}

Network* NetworkLibraryImplBase::FindNetworkByUniqueId(
    const std::string& unique_id) const {
  NetworkMap::const_iterator found = network_unique_id_map_.find(unique_id);
  if (found != network_unique_id_map_.end())
    return found->second;
  return NULL;
}

WirelessNetwork* NetworkLibraryImplBase::FindWirelessNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network &&
      (network->type() == TYPE_WIFI || network->type() == TYPE_WIMAX ||
           network->type() == TYPE_CELLULAR))
    return static_cast<WirelessNetwork*>(network);
  return NULL;
}

WifiNetwork* NetworkLibraryImplBase::FindWifiNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_WIFI)
    return static_cast<WifiNetwork*>(network);
  return NULL;
}

WimaxNetwork* NetworkLibraryImplBase::FindWimaxNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && (network->type() == TYPE_WIMAX))
    return static_cast<WimaxNetwork*>(network);
  return NULL;
}

CellularNetwork* NetworkLibraryImplBase::FindCellularNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_CELLULAR)
    return static_cast<CellularNetwork*>(network);
  return NULL;
}

VirtualNetwork* NetworkLibraryImplBase::FindVirtualNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_VPN)
    return static_cast<VirtualNetwork*>(network);
  return NULL;
}

Network* NetworkLibraryImplBase::FindRememberedFromNetwork(
    const Network* network) const {
  for (NetworkMap::const_iterator iter = remembered_network_map_.begin();
       iter != remembered_network_map_.end(); ++iter) {
    if (iter->second->unique_id() == network->unique_id())
      return iter->second;
  }
  return NULL;
}

Network* NetworkLibraryImplBase::FindRememberedNetworkByPath(
    const std::string& path) const {
  NetworkMap::const_iterator iter = remembered_network_map_.find(path);
  if (iter != remembered_network_map_.end())
    return iter->second;
  return NULL;
}

const base::DictionaryValue* NetworkLibraryImplBase::FindOncForNetwork(
    const std::string& unique_id) const {
  NetworkOncMap::const_iterator iter = network_onc_map_.find(unique_id);
  return iter != network_onc_map_.end() ? iter->second : NULL;
}

void NetworkLibraryImplBase::SignalCellularPlanPayment() {
  DCHECK(!HasRecentCellularPlanPayment());
  cellular_plan_payment_time_ = base::Time::Now();
}

bool NetworkLibraryImplBase::HasRecentCellularPlanPayment() {
  return (base::Time::Now() -
          cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
}

const std::string& NetworkLibraryImplBase::GetCellularHomeCarrierId() const {
  const NetworkDevice* cellular = FindCellularDevice();
  if (cellular)
    return cellular->home_provider_id();
  return EmptyString();
}

/////////////////////////////////////////////////////////////////////////////
// Profiles.

bool NetworkLibraryImplBase::HasProfileType(NetworkProfileType type) const {
  for (NetworkProfileList::const_iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    if ((*iter).type == type)
      return true;
  }
  return false;
}

NetworkLibraryImplBase::NetworkProfile::NetworkProfile(const std::string& p,
                                                       NetworkProfileType t)
    : path(p),
      type(t) {
}

NetworkLibraryImplBase::NetworkProfile::~NetworkProfile() {}

NetworkLibraryImplBase::ConnectData::ConnectData()
    : security(SECURITY_NONE),
      eap_method(EAP_METHOD_UNKNOWN),
      eap_auth(EAP_PHASE_2_AUTH_AUTO),
      eap_use_system_cas(false),
      save_credentials(false),
      profile_type(PROFILE_NONE) {
}

NetworkLibraryImplBase::ConnectData::~ConnectData() {}

const NetworkDevice* NetworkLibraryImplBase::FindDeviceByType(
    ConnectionType type) const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second && iter->second->type() == type)
      return iter->second;
  }
  return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// Connect to an existing network.

bool NetworkLibraryImplBase::CanConnectToNetwork(const Network* network) const {
  if (!HasProfileType(PROFILE_USER) && network->RequiresUserProfile())
    return false;
  return true;
}

// 1. Request a connection to an existing wifi network.
// Use |shared| to pass along the desired profile type.
void NetworkLibraryImplBase::ConnectToWifiNetwork(
    WifiNetwork* wifi, bool shared) {
  NetworkConnectStartWifi(wifi, shared ? PROFILE_SHARED : PROFILE_USER);
}

// 1. Request a connection to an existing wifi network.
void NetworkLibraryImplBase::ConnectToWifiNetwork(WifiNetwork* wifi) {
  NetworkConnectStartWifi(wifi, PROFILE_NONE);
}

// 1. Request a connection to an existing wimax network.
// Use |shared| to pass along the desired profile type.
void NetworkLibraryImplBase::ConnectToWimaxNetwork(
    WimaxNetwork* wimax, bool shared) {
  NetworkConnectStart(wimax, shared ? PROFILE_SHARED : PROFILE_USER);
}

// 1. Request a connection to an existing wimax network.
void NetworkLibraryImplBase::ConnectToWimaxNetwork(WimaxNetwork* wimax) {
  NetworkConnectStart(wimax, PROFILE_NONE);
}

// 1. Connect to a cellular network.
void NetworkLibraryImplBase::ConnectToCellularNetwork(
    CellularNetwork* cellular) {
  NetworkConnectStart(cellular, PROFILE_NONE);
}

// 1. Connect to an existing virtual network.
void NetworkLibraryImplBase::ConnectToVirtualNetwork(VirtualNetwork* vpn) {
  NetworkConnectStartVPN(vpn);
}

// 2. Start the connection.
void NetworkLibraryImplBase::NetworkConnectStartWifi(
    WifiNetwork* wifi, NetworkProfileType profile_type) {
  DCHECK(!wifi->connection_started());
  // This will happen if a network resets, gets out of range or is forgotten.
  if (wifi->user_passphrase_ != wifi->passphrase_ ||
      wifi->passphrase_required())
    wifi->SetPassphrase(wifi->user_passphrase_);
  // For enterprise 802.1X networks, always provide TPM PIN when available.
  // shill uses the PIN if it needs to access certificates in the TPM and
  // ignores it otherwise.
  if (wifi->encryption() == SECURITY_8021X) {
    // If the TPM initialization has not completed, GetTpmPin() will return
    // an empty value, in which case we do not want to clear the PIN since
    // that will cause shill to flag the network as unconfigured.
    // TODO(stevenjb): We may want to delay attempting to connect, or fail
    // immediately, rather than let the network layer attempt a connection.
    std::string tpm_pin = GetTpmPin();
    if (!tpm_pin.empty())
      wifi->SetCertificatePin(tpm_pin);
  }
  NetworkConnectStart(wifi, profile_type);
}

void NetworkLibraryImplBase::NetworkConnectStartVPN(VirtualNetwork* vpn) {
  // shill needs the TPM PIN for some VPN networks to access client
  // certificates, and ignores the PIN if it doesn't need them. Only set this
  // if the TPM is ready (see comment in NetworkConnectStartWifi).
  std::string tpm_pin = GetTpmPin();
  if (!tpm_pin.empty()) {
    std::string tpm_slot = GetTpmSlot();
    vpn->SetCertificateSlotAndPin(tpm_slot, tpm_pin);
  }
  NetworkConnectStart(vpn, PROFILE_NONE);
}

void NetworkLibraryImplBase::NetworkConnectStart(
    Network* network, NetworkProfileType profile_type) {
  DCHECK(network);
  DCHECK(!network->connection_started());
  // In order to be certain to trigger any notifications, set the connecting
  // state locally and notify observers. Otherwise there might be a state
  // change without a forced notify.
  network->set_connecting();
  // Distinguish between user-initiated connection attempts
  // and auto-connect.
  network->set_user_connect_state(USER_CONNECT_STARTED);
  NotifyNetworkManagerChanged(true);  // Forced update.
  VLOG(1) << "Requesting connect to network: " << network->name()
          << " profile type: " << profile_type;
  // Specify the correct profile for wifi networks (if specified or unset).
  if ((network->type() == TYPE_WIFI || network->type() == TYPE_WIMAX) &&
      (profile_type != PROFILE_NONE ||
       network->profile_type() == PROFILE_NONE)) {
    if (network->RequiresUserProfile())
      profile_type = PROFILE_USER;  // Networks with certs can not be shared.
    else if (profile_type == PROFILE_NONE)
      profile_type = PROFILE_SHARED;  // Other networks are shared by default.
    std::string profile_path = GetProfilePath(profile_type);
    if (!profile_path.empty()) {
      if (profile_path != network->profile_path())
        SetProfileType(network, profile_type);
    } else if (profile_type == PROFILE_USER) {
      // The user profile was specified but is not available (i.e. pre-login).
      // Add this network to the list of networks to move to the user profile
      // when it becomes available.
      VLOG(1) << "Queuing: " << network->name() << " to user_networks list.";
      user_networks_.push_back(network->service_path());
    }
  }
  CallConnectToNetwork(network);
}

// 3. Start the connection attempt for Network.
// Must Call NetworkConnectCompleted when the connection attempt completes.
// virtual void CallConnectToNetwork(Network* network) = 0;

// 4. Complete the connection.
void NetworkLibraryImplBase::NetworkConnectCompleted(
    Network* network, NetworkConnectStatus status) {
  DCHECK(network);
  if (status != CONNECT_SUCCESS) {
    // This will trigger the connection failed notification.
    // TODO(stevenjb): Remove if chromium-os:13203 gets fixed.
    network->SetState(STATE_FAILURE);
    if (status == CONNECT_BAD_PASSPHRASE) {
      network->SetError(ERROR_BAD_PASSPHRASE);
    } else {
      network->SetError(ERROR_CONNECT_FAILED);
    }
    NotifyNetworkManagerChanged(true);  // Forced update.
    NotifyNetworkChanged(network);
    VLOG(1) << "Error connecting to network: " << network->name()
            << " Status: " << status;
    return;
  }

  VLOG(1) << "Connected to network: " << network->name()
          << " State: " << network->state()
          << " Status: " << status;

  // If the user asked not to save credentials, shill will have
  // forgotten them.  Wipe our cache as well.
  if (!network->save_credentials())
    network->EraseCredentials();

  ClearActiveNetwork(network->type());
  UpdateActiveNetwork(network);

  // Notify observers.
  NotifyNetworkManagerChanged(true);  // Forced update.
  NotifyUserConnectionInitiated(network);
  NotifyNetworkChanged(network);
}

/////////////////////////////////////////////////////////////////////////////
// Request a network and connect to it.

// 1. Connect to an unconfigured or unlisted wifi network.
// This needs to request information about the named service.
// The connection attempt will occur in the callback.
void NetworkLibraryImplBase::ConnectToUnconfiguredWifiNetwork(
    const std::string& ssid,
    ConnectionSecurity security,
    const std::string& passphrase,
    const EAPConfigData* eap_config,
    bool save_credentials,
    bool shared) {
  // Store the connection data to be used by the callback.
  connect_data_.security = security;
  connect_data_.service_name = ssid;
  connect_data_.passphrase = passphrase;
  connect_data_.save_credentials = save_credentials;
  connect_data_.profile_type = shared ? PROFILE_SHARED : PROFILE_USER;
  if (security == SECURITY_8021X) {
    DCHECK(eap_config);
    connect_data_.service_name = ssid;
    connect_data_.eap_method = eap_config->method;
    connect_data_.eap_auth = eap_config->auth;
    connect_data_.server_ca_cert_nss_nickname =
        eap_config->server_ca_cert_nss_nickname;
    connect_data_.eap_use_system_cas = eap_config->use_system_cas;
    connect_data_.client_cert_pkcs11_id =
        eap_config->client_cert_pkcs11_id;
    connect_data_.eap_identity = eap_config->identity;
    connect_data_.eap_anonymous_identity = eap_config->anonymous_identity;
  }

  CallRequestWifiNetworkAndConnect(ssid, security);
}

// 1. Connect to a virtual network with a PSK.
void NetworkLibraryImplBase::ConnectToUnconfiguredVirtualNetwork(
    const std::string& service_name,
    const std::string& server_hostname,
    ProviderType provider_type,
    const VPNConfigData& config) {
  // Store the connection data to be used by the callback.
  connect_data_.service_name = service_name;
  connect_data_.server_hostname = server_hostname;
  connect_data_.psk_key = config.psk;
  connect_data_.server_ca_cert_nss_nickname =
      config.server_ca_cert_nss_nickname;
  connect_data_.client_cert_pkcs11_id = config.client_cert_pkcs11_id;
  connect_data_.username = config.username;
  connect_data_.passphrase = config.user_passphrase;
  connect_data_.otp = config.otp;
  connect_data_.group_name = config.group_name;
  connect_data_.save_credentials = config.save_credentials;
  CallRequestVirtualNetworkAndConnect(
      service_name, server_hostname, provider_type);
}

// 2. Requests a WiFi Network by SSID and security.
// Calls ConnectToWifiNetworkUsingConnectData if network request succeeds.
// virtual void CallRequestWifiNetworkAndConnect(
//     const std::string& ssid, ConnectionSecurity security) = 0;

// 2. Requests a Virtual Network by service name, etc.
// Calls ConnectToVirtualNetworkUsingConnectData if network request succeeds.
// virtual void CallRequestVirtualNetworkAndConnect(
//     const std::string& service_name,
//     const std::string& server_hostname,
//     ProviderType provider_type) = 0;

// 3. Sets network properties stored in ConnectData and calls
// NetworkConnectStart.
void NetworkLibraryImplBase::ConnectToWifiNetworkUsingConnectData(
    WifiNetwork* wifi) {
  ConnectData& data = connect_data_;
  if (wifi->name() != data.service_name) {
    LOG(WARNING) << "WiFi network name does not match ConnectData: "
                 << wifi->name() << " != " << data.service_name;
    return;
  }
  wifi->set_added(true);
  if (data.security == SECURITY_8021X) {
    // Enterprise 802.1X EAP network.
    wifi->SetEAPMethod(data.eap_method);
    wifi->SetEAPPhase2Auth(data.eap_auth);
    wifi->SetEAPServerCaCertNssNickname(data.server_ca_cert_nss_nickname);
    wifi->SetEAPUseSystemCAs(data.eap_use_system_cas);
    wifi->SetEAPClientCertPkcs11Id(data.client_cert_pkcs11_id);
    wifi->SetEAPIdentity(data.eap_identity);
    wifi->SetEAPAnonymousIdentity(data.eap_anonymous_identity);
    wifi->SetEAPPassphrase(data.passphrase);
    wifi->SetSaveCredentials(data.save_credentials);
  } else {
    // Ordinary, non-802.1X network.
    wifi->SetPassphrase(data.passphrase);
  }

  NetworkConnectStartWifi(wifi, data.profile_type);
}

// 3. Sets network properties stored in ConnectData and calls
// ConnectToVirtualNetwork.
void NetworkLibraryImplBase::ConnectToVirtualNetworkUsingConnectData(
    VirtualNetwork* vpn) {
  ConnectData& data = connect_data_;
  if (vpn->name() != data.service_name) {
    LOG(WARNING) << "Virtual network name does not match ConnectData: "
                 << vpn->name() << " != " << data.service_name;
    return;
  }

  // When a L2TP/IPsec certificate-based VPN is created, the VirtualNetwork
  // instance is created by NativeNetworkParser::CreateNetworkFromInfo().
  // At that point, the provider type is deduced based on the value of
  // client_cert_id_ of the VirtualNetwork instance, which hasn't been
  // updated to the value of connect_data_.client_cert_pkcs11_id. Thus,
  // the provider type is always incorrectly set to L2TP_IPSEC_PSK when
  // L2TP_IPSEC_USER_CERT is expected. Here we fix the provider type based
  // on connect_data_.client_cert_pkcs11_id.
  //
  // TODO(benchan): This is a quick and dirty workaround, we should refactor
  // the code to make the flow more straightforward. See crosbug.com/24636
  if (vpn->provider_type() == PROVIDER_TYPE_L2TP_IPSEC_PSK &&
      !connect_data_.client_cert_pkcs11_id.empty()) {
    vpn->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  }

  vpn->set_added(true);
  if (!data.server_hostname.empty())
    vpn->set_server_hostname(data.server_hostname);

  vpn->SetCACertNSS(data.server_ca_cert_nss_nickname);
  switch (vpn->provider_type()) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      vpn->SetL2TPIPsecPSKCredentials(
          data.psk_key, data.username, data.passphrase, data.group_name);
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT: {
      vpn->SetL2TPIPsecCertCredentials(
          data.client_cert_pkcs11_id,
          data.username, data.passphrase, data.group_name);
      break;
    }
    case PROVIDER_TYPE_OPEN_VPN: {
      vpn->SetOpenVPNCredentials(
          data.client_cert_pkcs11_id,
          data.username, data.passphrase, data.otp);
      break;
    }
    case PROVIDER_TYPE_MAX:
      NOTREACHED();
      break;
  }
  vpn->SetSaveCredentials(data.save_credentials);

  NetworkConnectStartVPN(vpn);
}

/////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::ForgetNetwork(const std::string& service_path) {
  // Remove network from remembered list and notify observers.
  DeleteRememberedNetwork(service_path);
  NotifyNetworkManagerChanged(true);  // Forced update.
}

/////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::EnableEthernetNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_ETHERNET, enable);
}

void NetworkLibraryImplBase::EnableWifiNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_WIFI, enable);
}

void NetworkLibraryImplBase::EnableMobileNetworkDevice(bool enable) {
  EnableWimaxNetworkDevice(enable);
  EnableCellularNetworkDevice(enable);
}

void NetworkLibraryImplBase::EnableWimaxNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_WIMAX, enable);
}

void NetworkLibraryImplBase::EnableCellularNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_CELLULAR, enable);
}

void NetworkLibraryImplBase::SwitchToPreferredNetwork() {
  // If current network (if any) is not preferred, check network service list to
  // see if the first not connected network is preferred and set to autoconnect.
  // If so, connect to it.
  if (!wifi_enabled() || (active_wifi_ && active_wifi_->preferred()))
    return;
  for (WifiNetworkVector::const_iterator it = wifi_networks_.begin();
      it != wifi_networks_.end(); ++it) {
    WifiNetwork* wifi = *it;
    if (wifi->connected() || wifi->connecting())  // Skip connected/connecting.
      continue;
    if (!wifi->preferred())  // All preferred networks are sorted in front.
      break;
    if (wifi->auto_connect()) {
      ConnectToWifiNetwork(wifi);
      break;
    }
  }
}

namespace {

class UserStringSubstitution : public onc::StringSubstitution {
 public:
  UserStringSubstitution() {}
  virtual bool GetSubstitute(std::string placeholder,
                             std::string* substitute) const {
    if (!UserManager::Get()->IsUserLoggedIn())
      return false;
    const User* logged_in_user = UserManager::Get()->GetLoggedInUser();
    if (placeholder == onc::substitutes::kLoginIDField)
      *substitute = logged_in_user->GetAccountName(false);
    else if (placeholder == onc::substitutes::kEmailField)
      *substitute = logged_in_user->email();
    else
      return false;
    return true;
  }
};

}  // namespace

bool NetworkLibraryImplBase::LoadOncNetworks(const std::string& onc_blob,
                                             const std::string& passphrase,
                                             onc::ONCSource source,
                                             bool allow_web_trust_from_policy) {
  VLOG(2) << __func__ << ": called on " << onc_blob;
  NetworkProfile* profile = NULL;
  bool from_policy = (source == onc::ONC_SOURCE_USER_POLICY ||
                      source == onc::ONC_SOURCE_DEVICE_POLICY);

  // Policies are applied to a specific Shill profile. User ONC import however
  // is applied to whatever profile Shill chooses. This should be the profile
  // that is already associated with a network and if no profile is associated
  // yet, it should be the user profile.
  if (from_policy) {
    profile = GetProfileForType(GetProfileTypeForSource(source));
    if (profile == NULL) {
      VLOG(2) << "Profile for ONC source " << onc::GetSourceAsString(source)
              << " doesn't exist.";
      return true;
    }
  }

  scoped_ptr<base::DictionaryValue> root_dict =
      onc::ReadDictionaryFromJson(onc_blob);
  if (root_dict.get() == NULL) {
    LOG(ERROR) << "ONC loaded from " << onc::GetSourceAsString(source)
               << " is not a valid JSON dictionary.";
    return false;
  }

  // Check and see if this is an encrypted ONC file. If so, decrypt it.
  std::string onc_type;
  root_dict->GetStringWithoutPathExpansion(onc::kType, &onc_type);
  if (onc_type == onc::kEncryptedConfiguration) {
    root_dict = onc::Decrypt(passphrase, *root_dict);
    if (root_dict.get() == NULL) {
      LOG(ERROR) << "Couldn't decrypt the ONC from "
                 << onc::GetSourceAsString(source);
      return false;
    }
  }

  // Validate the ONC dictionary. We are liberal and ignore unknown field
  // names and ignore invalid field names in kRecommended arrays.
  onc::Validator validator(false,  // Ignore unknown fields.
                           false,  // Ignore invalid recommended field names.
                           true,  // Fail on missing fields.
                           from_policy);
  validator.SetOncSource(source);

  onc::Validator::Result validation_result;
  root_dict = validator.ValidateAndRepairObject(
      &onc::kToplevelConfigurationSignature,
      *root_dict,
      &validation_result);

  if (from_policy) {
    UMA_HISTOGRAM_BOOLEAN("Enterprise.ONC.PolicyValidation",
                          validation_result == onc::Validator::VALID);
  }

  bool success = true;
  if (validation_result == onc::Validator::VALID_WITH_WARNINGS) {
    LOG(WARNING) << "ONC from " << onc::GetSourceAsString(source)
                 << " produced warnings.";
    success = false;
  } else if (validation_result == onc::Validator::INVALID ||
             root_dict == NULL) {
    LOG(ERROR) << "ONC from " << onc::GetSourceAsString(source)
               << " is invalid and couldn't be repaired.";
    return false;
  }

  const base::ListValue* certificates;
  bool has_certificates =
      root_dict->GetListWithoutPathExpansion(onc::kCertificates, &certificates);

  const base::ListValue* network_configs;
  bool has_network_configurations = root_dict->GetListWithoutPathExpansion(
      onc::kNetworkConfigurations,
      &network_configs);

  if (has_certificates) {
    VLOG(2) << "ONC file has " << certificates->GetSize() << " certificates";

    onc::CertificateImporter cert_importer(source, allow_web_trust_from_policy);
    if (cert_importer.ParseAndStoreCertificates(*certificates) !=
        onc::CertificateImporter::IMPORT_OK) {
      LOG(ERROR) << "Cannot parse some of the certificates in the ONC from "
                 << onc::GetSourceAsString(source);
      success = false;
    }
  }

  std::set<std::string> removal_ids;
  std::set<std::string>& network_ids(network_source_map_[source]);
  network_ids.clear();
  if (has_network_configurations) {
    VLOG(2) << "ONC file has " << network_configs->GetSize() << " networks";
    for (base::ListValue::const_iterator it(network_configs->begin());
         it != network_configs->end(); ++it) {
      const base::DictionaryValue* network;
      (*it)->GetAsDictionary(&network);

      bool marked_for_removal = false;
      network->GetBooleanWithoutPathExpansion(onc::kRemove,
                                              &marked_for_removal);

      std::string type;
      network->GetStringWithoutPathExpansion(onc::kType, &type);

      std::string guid;
      network->GetStringWithoutPathExpansion(onc::kGUID, &guid);

      if (source == onc::ONC_SOURCE_USER_IMPORT && marked_for_removal) {
        // User import supports the removal of networks by ID.
        removal_ids.insert(guid);
        continue;
      }

      // Don't configure a network that is supposed to be removed. For
      // policy-managed networks, the "remove" functionality of ONC is
      // irrelevant. Instead, in general, all previously configured networks
      // that are no longer configured are removed.
      if (marked_for_removal)
        continue;

      // Expand strings like LoginID
      base::DictionaryValue* expanded_network = network->DeepCopy();
      UserStringSubstitution substitution;
      onc::ExpandStringsInOncObject(onc::kNetworkConfigurationSignature,
                                    substitution,
                                    expanded_network);

      // Update the ONC map.
      const base::DictionaryValue*& entry = network_onc_map_[guid];
      delete entry;
      entry = expanded_network;

      // Normalize the ONC: Remove irrelevant fields.
      onc::Normalizer normalizer(true /* remove recommended fields */);
      scoped_ptr<base::DictionaryValue> normalized_network =
          normalizer.NormalizeObject(&onc::kNetworkConfigurationSignature,
                                     *expanded_network);

      // Configure the network.
      scoped_ptr<base::DictionaryValue> shill_dict =
          onc::TranslateONCObjectToShill(&onc::kNetworkConfigurationSignature,
                                         *normalized_network);

      // Set the ProxyConfig.
      const base::DictionaryValue* proxy_settings;
      if (normalized_network->GetDictionaryWithoutPathExpansion(
              onc::kProxySettings,
              &proxy_settings)) {
        scoped_ptr<base::DictionaryValue> proxy_config =
            onc::ConvertOncProxySettingsToProxyConfig(*proxy_settings);
        std::string proxy_json;
        base::JSONWriter::Write(proxy_config.get(), &proxy_json);
        shill_dict->SetStringWithoutPathExpansion(
            flimflam::kProxyConfigProperty,
            proxy_json);
      }

      // Set the UIData.
      scoped_ptr<NetworkUIData> ui_data =
          onc::CreateUIData(source, *normalized_network);
      base::DictionaryValue ui_data_dict;
      ui_data->FillDictionary(&ui_data_dict);
      std::string ui_data_json;
      base::JSONWriter::Write(&ui_data_dict, &ui_data_json);
      shill_dict->SetStringWithoutPathExpansion(flimflam::kUIDataProperty,
                                                ui_data_json);

      // Set the appropriate profile for |source|.
      if (profile != NULL) {
        shill_dict->SetStringWithoutPathExpansion(flimflam::kProfileProperty,
                                                  profile->path);
      }

      // For Ethernet networks, apply them to the current Ethernet service.
      if (type == onc::kEthernet) {
        const EthernetNetwork* ethernet = ethernet_network();
        if (ethernet) {
          CallConfigureService(ethernet->unique_id(), shill_dict.get());
        } else {
          LOG(WARNING) << "Tried to import ONC with an Ethernet network when "
                       << "there is no active Ethernet connection.";
        }
      } else {
        CallConfigureService(guid, shill_dict.get());
      }

      // Store the network's identifier. The identifiers are later used to clean
      // out any previously-existing networks that had been configured through
      // policy but are no longer specified in the updated ONC blob.
      network_ids.insert(guid);
    }
  }

  if (from_policy) {
    // For policy-managed networks, go through the list of existing remembered
    // networks and clean out the ones that no longer have a definition in the
    // ONC blob. We first collect the networks and do the actual deletion later
    // because ForgetNetwork() changes the remembered network vectors.
    ForgetNetworksById(source, network_ids, false);
  } else if (source == onc::ONC_SOURCE_USER_IMPORT && !removal_ids.empty()) {
    ForgetNetworksById(source, removal_ids, true);
  }

  return success;
}

////////////////////////////////////////////////////////////////////////////
// Testing functions.

bool NetworkLibraryImplBase::SetActiveNetwork(
    ConnectionType type, const std::string& service_path) {
  Network* network = NULL;
  if (!service_path.empty())
    network = FindNetworkByPath(service_path);
  if (network && network->type() != type) {
    LOG(WARNING) << "SetActiveNetwork type mismatch for: " << network->name();
    return false;
  }

  ClearActiveNetwork(type);

  if (!network)
    return true;

  // Set |network| to active.
  UpdateActiveNetwork(network);
  return true;
}

////////////////////////////////////////////////////////////////////////////
// Network list management functions.

// Note: sometimes shill still returns networks when the device type is
// disabled. Always check the appropriate enabled() state before adding
// networks to a list or setting an active network so that we do not show them
// in the UI.

// This relies on services being requested from shill in priority order,
// and the updates getting processed and received in order.
void NetworkLibraryImplBase::UpdateActiveNetwork(Network* network) {
  network->set_is_active(true);
  ConnectionType type(network->type());
  if (type == TYPE_ETHERNET) {
    if (ethernet_enabled()) {
      // Set ethernet_ to the first connected ethernet service, or the first
      // disconnected ethernet service if none are connected.
      if (ethernet_ == NULL || !ethernet_->connected()) {
        ethernet_ = static_cast<EthernetNetwork*>(network);
        VLOG(2) << "Active ethernet -> " << ethernet_->name();
      }
    }
  } else if (type == TYPE_WIFI) {
    if (wifi_enabled()) {
      // Set active_wifi_ to the first connected or connecting wifi service.
      if (active_wifi_ == NULL && network->connecting_or_connected()) {
        active_wifi_ = static_cast<WifiNetwork*>(network);
        VLOG(2) << "Active wifi -> " << active_wifi_->name();
      }
    }
  } else if (type == TYPE_CELLULAR) {
    if (cellular_enabled()) {
      // Set active_cellular_ to first connected/connecting celluar service.
      if (active_cellular_ == NULL && network->connecting_or_connected()) {
        active_cellular_ = static_cast<CellularNetwork*>(network);
        VLOG(2) << "Active cellular -> " << active_cellular_->name();
      }
    }
  } else if (type == TYPE_WIMAX) {
    if (wimax_enabled()) {
      // Set active_wimax_ to first connected/connecting wimax service.
      if (active_wimax_ == NULL && network->connecting_or_connected()) {
        active_wimax_ = static_cast<WimaxNetwork*>(network);
        VLOG(2) << "Active wimax -> " << active_wimax_->name();
      }
    }
  } else if (type == TYPE_VPN) {
    // Set active_virtual_ to the first connected or connecting vpn service. {
    if (active_virtual_ == NULL && network->connecting_or_connected()) {
      active_virtual_ = static_cast<VirtualNetwork*>(network);
      VLOG(2) << "Active virtual -> " << active_virtual_->name();
    }
  }
}

void NetworkLibraryImplBase::ClearActiveNetwork(ConnectionType type) {
  // Clear any existing active network matching |type|.
  for (NetworkMap::iterator iter = network_map_.begin();
       iter != network_map_.end(); ++iter) {
    Network* other = iter->second;
    if (other->type() == type)
      other->set_is_active(false);
  }
  switch (type) {
    case TYPE_ETHERNET:
      ethernet_ = NULL;
      break;
    case TYPE_WIFI:
      active_wifi_ = NULL;
      break;
    case TYPE_CELLULAR:
      active_cellular_ = NULL;
      break;
    case TYPE_WIMAX:
      active_wimax_ = NULL;
      break;
    case TYPE_VPN:
      active_virtual_ = NULL;
      break;
    default:
      break;
  }
}

void NetworkLibraryImplBase::AddNetwork(Network* network) {
  std::pair<NetworkMap::iterator, bool> result =
      network_map_.insert(std::make_pair(network->service_path(), network));
  DCHECK(result.second);  // Should only get called with new network.
  VLOG(2) << "Adding Network: " << network->service_path()
          << " (" << network->name() << ")";
  ConnectionType type(network->type());
  if (type == TYPE_WIFI) {
    if (wifi_enabled())
      wifi_networks_.push_back(static_cast<WifiNetwork*>(network));
  } else if (type == TYPE_CELLULAR) {
    if (cellular_enabled())
      cellular_networks_.push_back(static_cast<CellularNetwork*>(network));
  } else if (type == TYPE_WIMAX) {
    if (wimax_enabled())
      wimax_networks_.push_back(static_cast<WimaxNetwork*>(network));
  } else if (type == TYPE_VPN) {
    virtual_networks_.push_back(static_cast<VirtualNetwork*>(network));
  }
  // Do not set the active network here. Wait until we parse the network.
}

// Deletes a network. It must already be removed from any lists.
void NetworkLibraryImplBase::DeleteNetwork(Network* network) {
  CHECK(network_map_.find(network->service_path()) == network_map_.end());
  delete network;
}

void NetworkLibraryImplBase::ForgetNetworksById(
    onc::ONCSource source,
    std::set<std::string> ids,
    bool if_found) {
  std::vector<std::string> to_be_forgotten;
  for (WifiNetworkVector::iterator i = remembered_wifi_networks_.begin();
       i != remembered_wifi_networks_.end(); ++i) {
    WifiNetwork* wifi_network = *i;
    if (wifi_network->ui_data().onc_source() == source &&
        if_found == (ids.find(wifi_network->unique_id()) != ids.end()))
      to_be_forgotten.push_back(wifi_network->service_path());
  }

  for (VirtualNetworkVector::iterator i = remembered_virtual_networks_.begin();
       i != remembered_virtual_networks_.end(); ++i) {
    VirtualNetwork* virtual_network = *i;
    if (virtual_network->ui_data().onc_source() == source &&
        if_found == (ids.find(virtual_network->unique_id()) != ids.end()))
      to_be_forgotten.push_back(virtual_network->service_path());
  }

  for (std::vector<std::string>::const_iterator i = to_be_forgotten.begin();
       i != to_be_forgotten.end(); ++i) {
    ForgetNetwork(*i);
  }
}

bool NetworkLibraryImplBase::ValidateRememberedNetwork(Network* network) {
  std::pair<NetworkMap::iterator, bool> result =
      remembered_network_map_.insert(
          std::make_pair(network->service_path(), network));
  DCHECK(result.second);  // Should only get called with new network.

  // See if this is a policy-configured network that has meanwhile been removed.
  // This situation may arise when the full list of remembered networks is not
  // available to LoadOncNetworks(), which can happen due to the asynchronous
  // communication between shill and NetworkLibrary. Just tell shill to
  // delete the network now.
  const onc::ONCSource source = network->ui_data().onc_source();
  if (source == onc::ONC_SOURCE_USER_POLICY ||
      source == onc::ONC_SOURCE_DEVICE_POLICY) {
    NetworkSourceMap::const_iterator network_id_set(
        network_source_map_.find(source));
    if (network_id_set != network_source_map_.end() &&
        network_id_set->second.find(network->unique_id()) ==
            network_id_set->second.end()) {
      DeleteRememberedNetwork(network->service_path());
      return false;
    }
  }

  return true;
}

bool NetworkLibraryImplBase::ValidateAndAddRememberedNetwork(Network* network) {
  if (!ValidateRememberedNetwork(network))
    return false;

  if (network->type() == TYPE_WIFI) {
    remembered_wifi_networks_.push_back(
        static_cast<WifiNetwork*>(network));
  } else if (network->type() == TYPE_VPN) {
    remembered_virtual_networks_.push_back(
        static_cast<VirtualNetwork*>(network));
  } else {
    NOTREACHED();
  }

  VLOG(1) << "ValidateAndAddRememberedNetwork: " << network->service_path();
  return true;
}

void NetworkLibraryImplBase::DeleteRememberedNetwork(
    const std::string& service_path) {
  NetworkMap::iterator found = remembered_network_map_.find(service_path);
  if (found == remembered_network_map_.end()) {
    LOG(WARNING) << "Attempt to delete non-existent remembered network: "
                 << service_path;
    return;
  }
  Network* remembered_network = found->second;
  VLOG(1) << "Deleting remembered network: "
          << remembered_network->service_path();

  // Update any associated network service before removing from profile
  // so that shill doesn't recreate the service (e.g. when we disconenct it).
  Network* network = FindNetworkByUniqueId(remembered_network->unique_id());
  if (network) {
    // Clear the stored credentials for any forgotten networks.
    network->EraseCredentials();
    network->ClearUIData();
    SetProfileType(network, PROFILE_NONE);
    // Remove VPN from list of networks.
    if (network->type() == TYPE_VPN)
      CallRemoveNetwork(network);
  } else {
    // Network is not in service list.
    VLOG(2) << "Remembered Network not in service list: "
            << remembered_network->unique_id();
  }

  // Delete remembered network from lists.
  remembered_network_map_.erase(found);

  if (remembered_network->type() == TYPE_WIFI) {
    WifiNetworkVector::iterator iter = std::find(
        remembered_wifi_networks_.begin(),
        remembered_wifi_networks_.end(),
        remembered_network);
    if (iter != remembered_wifi_networks_.end())
      remembered_wifi_networks_.erase(iter);
  } else if (remembered_network->type() == TYPE_VPN) {
    VirtualNetworkVector::iterator iter = std::find(
        remembered_virtual_networks_.begin(),
        remembered_virtual_networks_.end(),
        remembered_network);
    if (iter != remembered_virtual_networks_.end())
      remembered_virtual_networks_.erase(iter);
  }

  // Delete remembered network from all profiles it is in.
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    NetworkProfile::ServiceList::iterator found =
        profile.services.find(remembered_network->service_path());
    if (found != profile.services.end()) {
      VLOG(1) << "Deleting: " << remembered_network->service_path()
              << " From: " << profile.path;
      CallDeleteRememberedNetwork(profile.path,
                                  remembered_network->service_path());
      profile.services.erase(found);
    }
  }

  // Remove the ONC blob for the network, if present.
  NetworkOncMap::iterator onc_map_entry =
      network_onc_map_.find(remembered_network->unique_id());
  if (onc_map_entry != network_onc_map_.end()) {
    delete onc_map_entry->second;
    network_onc_map_.erase(onc_map_entry);
  }

  delete remembered_network;
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::ClearNetworks() {
  network_map_.clear();
  network_unique_id_map_.clear();
  ethernet_ = NULL;
  active_wifi_ = NULL;
  active_cellular_ = NULL;
  active_wimax_ = NULL;
  active_virtual_ = NULL;
  wifi_networks_.clear();
  cellular_networks_.clear();
  wimax_networks_.clear();
  virtual_networks_.clear();
}

void NetworkLibraryImplBase::DeleteRememberedNetworks() {
  STLDeleteValues(&remembered_network_map_);
  remembered_network_map_.clear();
  remembered_wifi_networks_.clear();
  remembered_virtual_networks_.clear();
}

void NetworkLibraryImplBase::DeleteDevice(const std::string& device_path) {
  NetworkDeviceMap::iterator found = device_map_.find(device_path);
  if (found == device_map_.end()) {
    LOG(WARNING) << "Attempt to delete non-existent device: "
                 << device_path;
    return;
  }
  VLOG(2) << " Deleting device: " << device_path;
  NetworkDevice* device = found->second;
  device_map_.erase(found);
  delete device;
  DeleteDeviceFromDeviceObserversMap(device_path);
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::AddProfile(const std::string& profile_path,
                                        NetworkProfileType profile_type) {
  VLOG(1) << "Adding profile " << profile_path;
  profile_list_.push_back(NetworkProfile(profile_path, profile_type));
  // Check to see if we connected to any networks before a user profile was
  // available (i.e. before login), but unchecked the "Share" option (i.e.
  // the desired pofile is the user profile). Move these networks to the
  // user profile when it becomes available.
  if (profile_type == PROFILE_USER && !user_networks_.empty()) {
    for (std::list<std::string>::iterator iter2 = user_networks_.begin();
         iter2 != user_networks_.end(); ++iter2) {
      Network* network = FindNetworkByPath(*iter2);
      if (network && network->profile_path() != profile_path)
        network->SetProfilePath(profile_path);
    }
    user_networks_.clear();
  }
}

NetworkLibraryImplBase::NetworkProfile*
NetworkLibraryImplBase::GetProfileForType(NetworkProfileType type) {
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.type == type)
      return &profile;
  }
  return NULL;
}

void NetworkLibraryImplBase::SetProfileType(
    Network* network, NetworkProfileType type) {
  if (type == PROFILE_NONE) {
    network->SetProfilePath(std::string());
    network->set_profile_type(PROFILE_NONE);
  } else {
    std::string profile_path = GetProfilePath(type);
    if (!profile_path.empty()) {
      network->SetProfilePath(profile_path);
      network->set_profile_type(type);
    } else {
      LOG(WARNING) << "Profile type not found: " << type;
      network->set_profile_type(PROFILE_NONE);
    }
  }
}

void NetworkLibraryImplBase::SetProfileTypeFromPath(Network* network) {
  if (network->profile_path().empty()) {
    network->set_profile_type(PROFILE_NONE);
    return;
  }
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.path == network->profile_path()) {
      network->set_profile_type(profile.type);
      return;
    }
  }
  LOG(WARNING) << "Profile path not found: " << network->profile_path();
  network->set_profile_type(PROFILE_NONE);
}

std::string NetworkLibraryImplBase::GetProfilePath(NetworkProfileType type) {
  std::string profile_path;
  NetworkProfile* profile = GetProfileForType(type);
  if (profile)
    profile_path = profile->path;
  return profile_path;
}

////////////////////////////////////////////////////////////////////////////
// Notifications.

void NetworkLibraryImplBase::NotifyNetworkProfileObservers() {
  FOR_EACH_OBSERVER(NetworkProfileObserver,
                    network_profile_observers_,
                    OnProfileListChanged());
}

// We call this any time something in NetworkLibrary changes.
// TODO(stevenjb): We should consider breaking this into multiple
// notifications, e.g. connection state, devices, services, etc.
void NetworkLibraryImplBase::NotifyNetworkManagerChanged(bool force_update) {
  // Cancel any pending signals.
  notify_manager_weak_factory_.InvalidateWeakPtrs();
  if (force_update) {
    // Signal observers now.
    SignalNetworkManagerObservers();
  } else {
    // Schedule a delayed signal to limit the frequency of notifications.
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(&NetworkLibraryImplBase::SignalNetworkManagerObservers,
                   notify_manager_weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kNetworkNotifyDelayMs));
  }
}

void NetworkLibraryImplBase::SignalNetworkManagerObservers() {
  FOR_EACH_OBSERVER(NetworkManagerObserver,
                    network_manager_observers_,
                    OnNetworkManagerChanged(this));
  // Clear notification flags.
  for (NetworkMap::iterator iter = network_map_.begin();
       iter != network_map_.end(); ++iter) {
    iter->second->set_notify_failure(false);
  }
}

void NetworkLibraryImplBase::NotifyNetworkChanged(const Network* network) {
  DCHECK(network);
  VLOG(2) << "Network changed: " << network->name();
  NetworkObserverMap::const_iterator iter = network_observers_.find(
      network->service_path());
  if (iter != network_observers_.end()) {
    FOR_EACH_OBSERVER(NetworkObserver,
                      *(iter->second),
                      OnNetworkChanged(this, network));
  } else if (IsCros()) {
    LOG(ERROR) << "Unexpected signal for unobserved network: "
               << network->name();
  }
}

void NetworkLibraryImplBase::NotifyNetworkDeviceChanged(
    NetworkDevice* device, PropertyIndex index) {
  DCHECK(device);
  VLOG(2) << "Network device changed: " << device->name();
  NetworkDeviceObserverMap::const_iterator iter =
      network_device_observers_.find(device->device_path());
  if (iter != network_device_observers_.end()) {
    NetworkDeviceObserverList* device_observer_list = iter->second;
    if (index == PROPERTY_INDEX_FOUND_NETWORKS) {
      FOR_EACH_OBSERVER(NetworkDeviceObserver,
                        *device_observer_list,
                        OnNetworkDeviceFoundNetworks(this, device));
    } else if (index == PROPERTY_INDEX_SIM_LOCK) {
      FOR_EACH_OBSERVER(NetworkDeviceObserver,
                        *device_observer_list,
                        OnNetworkDeviceSimLockChanged(this, device));
    }
    FOR_EACH_OBSERVER(NetworkDeviceObserver,
                      *device_observer_list,
                      OnNetworkDeviceChanged(this, device));
  } else {
    LOG(ERROR) << "Unexpected signal for unobserved device: "
               << device->name();
  }
}

void NetworkLibraryImplBase::NotifyPinOperationCompleted(
    PinOperationError error) {
  FOR_EACH_OBSERVER(PinOperationObserver,
                    pin_operation_observers_,
                    OnPinOperationCompleted(this, error));
  sim_operation_ = SIM_OPERATION_NONE;
}

void NetworkLibraryImplBase::NotifyUserConnectionInitiated(
    const Network* network) {
  FOR_EACH_OBSERVER(UserActionObserver,
                    user_action_observers_,
                    OnConnectionInitiated(this, network));
}

//////////////////////////////////////////////////////////////////////////////
// Pin related functions.

void NetworkLibraryImplBase::GetTpmInfo() {
  // Avoid making multiple synchronous D-Bus calls to cryptohome by caching
  // the TPM PIN, which does not change during a session.
  if (tpm_pin_.empty()) {
    if (crypto::IsTPMTokenReady()) {
      std::string tpm_label;
      crypto::GetTPMTokenInfo(&tpm_label, &tpm_pin_);
      // VLOG(1) << "TPM Label: " << tpm_label << ", PIN: " << tpm_pin_;
      // TODO(stevenjb): GetTPMTokenInfo returns a label, but the network
      // code expects a slot ID. See chromium-os:17998.
      // For now, use a hard coded, well known slot instead.
      const char kHardcodedTpmSlot[] = "0";
      tpm_slot_ = kHardcodedTpmSlot;
    } else if (IsCros()) {
      LOG(WARNING) << "TPM token not ready";
    }
  }
}

const std::string& NetworkLibraryImplBase::GetTpmSlot() {
  GetTpmInfo();
  return tpm_slot_;
}

const std::string& NetworkLibraryImplBase::GetTpmPin() {
  GetTpmInfo();
  return tpm_pin_;
}

}  // namespace chromeos
