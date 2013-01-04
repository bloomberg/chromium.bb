// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"  // for debug output only.
#include "base/string_number_conversions.h"
#include "base/utf_string_conversion_utils.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/certificate_pattern.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros/network_library_impl_cros.h"
#include "chrome/browser/chromeos/cros/network_library_impl_stub.h"
#include "chrome/common/net/url_util.h"
#include "chrome/common/net/x509_certificate_model.h"
#include "chromeos/network/cros_network_functions.h"
#include "content/public/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;

////////////////////////////////////////////////////////////////////////////////
// Implementation notes.
// NetworkLibraryImpl manages a series of classes that describe network devices
// and services:
//
// NetworkDevice: e.g. ethernet, wifi modem, cellular modem
//  device_map_: canonical map<path, NetworkDevice*> for devices
//
// Network: a network service ("network").
//  network_map_: canonical map<path, Network*> for all visible networks.
//  EthernetNetwork
//   ethernet_: EthernetNetwork* to the active ethernet network in network_map_.
//  WirelessNetwork: a WiFi or Cellular Network.
//  WifiNetwork
//   active_wifi_: WifiNetwork* to the active wifi network in network_map_.
//   wifi_networks_: ordered vector of WifiNetwork* entries in network_map_,
//       in descending order of importance.
//  CellularNetwork
//   active_cellular_: Cellular version of wifi_.
//   cellular_networks_: Cellular version of wifi_.
// network_unique_id_map_: map<unique_id, Network*> for all visible networks.
// remembered_network_map_: a canonical map<path, Network*> for all networks
//     remembered in the active Profile ("favorites").
// remembered_network_unique_id_map_: map<unique_id, Network*> for all
//     remembered networks.
// remembered_wifi_networks_: ordered vector of WifiNetwork* entries in
//     remembered_network_map_, in descending order of preference.
// remembered_virtual_networks_: ordered vector of VirtualNetwork* entries in
//     remembered_network_map_, in descending order of preference.
//
// network_manager_monitor_: a handle to the libcros network Manager handler.
// NetworkManagerStatusChanged: This handles all messages from the Manager.
//   Messages are parsed here and the appropriate updates are then requested.
//
// UpdateNetworkServiceList: This is the primary Manager handler. It handles
//  the "Services" message which list all visible networks. The handler
//  rebuilds the network lists without destroying existing Network structures,
//  then requests neccessary updates to be fetched asynchronously from
//  libcros (RequestNetworkServiceProperties).
//
// TODO(stevenjb): Document cellular data plan handlers.
//
// AddNetworkObserver: Adds an observer for a specific network.
// UpdateNetworkStatus: This handles changes to a monitored service, typically
//     changes to transient states like Strength. (Note: also updates State).
//
// AddNetworkDeviceObserver: Adds an observer for a specific device.
//                           Will be called on any device property change.
// UpdateNetworkDeviceStatus: Handles changes to a monitored device, like
//     SIM lock state and updates device state.
//
// All *Pin(...) methods use internal callback that would update cellular
//    device state once async call is completed and notify all device observers.
//
////////////////////////////////////////////////////////////////////////////////

namespace chromeos {

// Local constants.
namespace {

// Default value of the SIM unlock retries count. It is updated to the real
// retries count once cellular device with SIM card is initialized.
// If cellular device doesn't have SIM card, then retries are never used.
const int kDefaultSimUnlockRetriesCount = 999;

// Redirect extension url for POST-ing url parameters to mobile account status
// sites.
const char kRedirectExtensionPage[] =
    "chrome-extension://iadeocfgjdjdmpenejdbfeaocpbikmab/redirect.html?"
    "autoPost=1";

////////////////////////////////////////////////////////////////////////////////
// Misc.

// Erase the memory used by a string, then clear it.
void WipeString(std::string* str) {
  str->assign(str->size(), '\0');
  str->clear();
}

bool EnsureCrosLoaded() {
  if (!CrosLibrary::Get()->libcros_loaded()) {
    return false;
  } else {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << "chromeos_network calls made from non UI thread!";
    return true;
  }
}

void ValidateUTF8(const std::string& str, std::string* output) {
  output->clear();

  for (int32 index = 0; index < static_cast<int32>(str.size()); ++index) {
    uint32 code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(str.c_str(), str.size(),
                                                      &index, &code_point_out);
    if (is_unicode_char && (code_point_out >= 0x20))
      base::WriteUnicodeCharacter(code_point_out, output);
    else
      // Puts REPLACEMENT CHARACTER (U+FFFD) if character is not readable UTF-8
      base::WriteUnicodeCharacter(0xFFFD, output);
  }
}

std::string ConnectionStateString(ConnectionState state) {
  switch (state) {
    case STATE_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNKNOWN);
    case STATE_IDLE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_IDLE);
    case STATE_CARRIER:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CARRIER);
    case STATE_ASSOCIATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_ASSOCIATION);
    case STATE_CONFIGURATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CONFIGURATION);
    case STATE_READY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_READY);
    case STATE_DISCONNECT:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_DISCONNECT);
    case STATE_FAILURE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_FAILURE);
    case STATE_ACTIVATION_FAILURE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_STATE_ACTIVATION_FAILURE);
    case STATE_PORTAL:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_PORTAL);
    case STATE_ONLINE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_ONLINE);
    case STATE_CONNECT_REQUESTED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_STATE_CONNECT_REQUESTED);
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FoundCellularNetwork

FoundCellularNetwork::FoundCellularNetwork() {}

FoundCellularNetwork::~FoundCellularNetwork() {}

////////////////////////////////////////////////////////////////////////////////
// NetworkDevice

NetworkDevice::NetworkDevice(const std::string& device_path)
    : device_path_(device_path),
      type_(TYPE_UNKNOWN),
      scanning_(false),
      sim_lock_state_(SIM_UNKNOWN),
      sim_retries_left_(kDefaultSimUnlockRetriesCount),
      sim_pin_required_(SIM_PIN_REQUIRE_UNKNOWN),
      sim_present_(false),
      prl_version_(0),
      data_roaming_allowed_(false),
      support_network_scan_(false),
      device_parser_(new NativeNetworkDeviceParser) {
}

NetworkDevice::~NetworkDevice() {}

void NetworkDevice::SetNetworkDeviceParser(NetworkDeviceParser* parser) {
  device_parser_.reset(parser);
}

void NetworkDevice::ParseInfo(const DictionaryValue& info) {
  if (device_parser_.get())
    device_parser_->UpdateDeviceFromInfo(info, this);
}

bool NetworkDevice::UpdateStatus(const std::string& key,
                                 const base::Value& value,
                                 PropertyIndex* index) {
  if (device_parser_.get())
    return device_parser_->UpdateStatus(key, value, this, index);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// Network

Network::Network(const std::string& service_path,
                 ConnectionType type)
    : state_(STATE_UNKNOWN),
      error_(ERROR_NO_ERROR),
      connectable_(true),
      connection_started_(false),
      is_active_(false),
      priority_(kPriorityNotSet),
      auto_connect_(false),
      save_credentials_(false),
      priority_order_(0),
      added_(false),
      notify_failure_(false),
      profile_type_(PROFILE_NONE),
      service_path_(service_path),
      type_(type) {
}

Network::~Network() {
  for (PropertyMap::const_iterator props = property_map_.begin();
       props != property_map_.end(); ++props) {
     delete props->second;
  }
}

Network::ProxyOncConfig::ProxyOncConfig() : type(PROXY_ONC_DIRECT) {}

void Network::SetNetworkParser(NetworkParser* parser) {
  network_parser_.reset(parser);
}

void Network::UpdatePropertyMap(PropertyIndex index, const base::Value* value) {
  if (!value) {
    // Clear the property if |value| is NULL.
    PropertyMap::iterator iter(property_map_.find(index));
    if (iter != property_map_.end()) {
      delete iter->second;
      property_map_.erase(iter);
    }
    return;
  }

  // Add the property to property_map_.  Delete previous value if necessary.
  Value*& entry = property_map_[index];
  delete entry;
  entry = value->DeepCopy();
  if (VLOG_IS_ON(3)) {
    std::string value_json;
    base::JSONWriter::WriteWithOptions(value,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &value_json);
    VLOG(3) << "Updated property map on network: "
            << unique_id() << "[" << index << "] = " << value_json;
  }
}

bool Network::GetProperty(PropertyIndex index,
                          const base::Value** value) const {
  PropertyMap::const_iterator i = property_map_.find(index);
  if (i == property_map_.end())
    return false;
  if (value != NULL)
    *value = i->second;
  return true;
}

// static
Network* Network::CreateForTesting(ConnectionType type) {
  return new Network("fake_service_path", type);
}

void Network::SetState(ConnectionState new_state) {
  if (new_state == state_)
    return;
  if (state_ == STATE_CONNECT_REQUESTED && new_state == STATE_IDLE) {
    // CONNECT_REQUESTED is set internally. Shill does not update the
    // state immediately, so ignore any Idle state updates sent while a
    // connection attempt is in progress.
    VLOG(2) << "Ignoring idle state change after connection request.";
    return;
  }
  ConnectionState old_state = state_;
  VLOG(2) << "Entering new state: " << ConnectionStateString(new_state);
  state_ = new_state;
  if (new_state == STATE_FAILURE) {
    VLOG(1) << service_path() << ": Detected Failure state.";
    if (old_state != STATE_UNKNOWN && old_state != STATE_IDLE) {
      // New failure, the user needs to be notified.
      // Transition STATE_IDLE -> STATE_FAILURE sometimes happens on resume
      // but is not an actual failure as network device is not ready yet.
      notify_failure_ = true;
      // Normally error_ should be set, but if it is not we need to set it to
      // something here so that the retry logic will be triggered.
      if (error_ == ERROR_NO_ERROR) {
        VLOG(2) << "Detected NO_ERROR error state.  Setting to UNKNOWN.";
        error_ = ERROR_UNKNOWN;
      }
    }
  } else if (new_state == STATE_IDLE && IsConnectingState(old_state)
             && connection_started()) {
    // If we requested a connect and never went through a connected state,
    // treat it as a failure.
    VLOG(1) << service_path() << ": Inferring Failure state.";
    notify_failure_ = true;
    error_ = ERROR_UNKNOWN;
  } else if (new_state != STATE_UNKNOWN) {
    notify_failure_ = false;
    // State changed, so refresh IP address.
    InitIPAddress();
  }
  VLOG(1) << name() << ".State [" << service_path() << "]: " << GetStateString()
          << " (was: " << ConnectionStateString(old_state) << ")";
  if (!IsConnectingState(new_state) && new_state != STATE_UNKNOWN)
    set_connection_started(false);
}

void Network::SetError(ConnectionError error) {
  error_ = error;
  if (error == ERROR_NO_ERROR)
    notify_failure_ = false;
}

void Network::SetName(const std::string& name) {
  std::string name_utf8;
  ValidateUTF8(name, &name_utf8);
  set_name(name_utf8);
}

void Network::ParseInfo(const DictionaryValue& info) {
  if (network_parser_.get())
    network_parser_->UpdateNetworkFromInfo(info, this);
}

void Network::EraseCredentials() {
}

void Network::CalculateUniqueId() {
  unique_id_ = name_;
}

bool Network::RequiresUserProfile() const {
  return false;
}

void Network::CopyCredentialsFromRemembered(Network* remembered) {
}

void Network::SetValueProperty(const char* prop, const base::Value& value) {
  DCHECK(prop);
  if (!EnsureCrosLoaded())
    return;
  CrosSetNetworkServiceProperty(service_path_, prop, value);
}

void Network::ClearProperty(const char* prop) {
  DCHECK(prop);
  if (!EnsureCrosLoaded())
    return;
  CrosClearNetworkServiceProperty(service_path_, prop);
}

void Network::SetStringProperty(
    const char* prop, const std::string& str, std::string* dest) {
  if (dest)
    *dest = str;
  SetValueProperty(prop, base::StringValue(str));
}

void Network::SetOrClearStringProperty(const char* prop,
                                       const std::string& str,
                                       std::string* dest) {
  if (str.empty()) {
    ClearProperty(prop);
    if (dest)
      dest->clear();
  } else {
    SetStringProperty(prop, str, dest);
  }
}

void Network::SetBooleanProperty(const char* prop, bool b, bool* dest) {
  if (dest)
    *dest = b;
  SetValueProperty(prop, base::FundamentalValue(b));
}

void Network::SetIntegerProperty(const char* prop, int i, int* dest) {
  if (dest)
    *dest = i;
  SetValueProperty(prop, base::FundamentalValue(i));
}

void Network::SetPreferred(bool preferred) {
  if (preferred) {
    SetIntegerProperty(
        flimflam::kPriorityProperty, kPriorityPreferred, &priority_);
  } else {
    ClearProperty(flimflam::kPriorityProperty);
    priority_ = kPriorityNotSet;
  }
}

void Network::SetAutoConnect(bool auto_connect) {
  SetBooleanProperty(
      flimflam::kAutoConnectProperty, auto_connect, &auto_connect_);
}

void Network::SetSaveCredentials(bool save_credentials) {
  SetBooleanProperty(
      flimflam::kSaveCredentialsProperty, save_credentials, &save_credentials_);
}

void Network::ClearUIData() {
  ui_data_ = NetworkUIData();
  ClearProperty(flimflam::kUIDataProperty);
}

void Network::AttemptConnection(const base::Closure& closure) {
  // By default, just invoke the closure right away.  Some subclasses
  // (Wifi, VPN, etc.) override to do more work.
  closure.Run();
}

void Network::SetProfilePath(const std::string& profile_path) {
  VLOG(1) << "Setting profile for: " << name_ << " to: " << profile_path;
  SetOrClearStringProperty(
      flimflam::kProfileProperty, profile_path, &profile_path_);
}

std::string Network::GetStateString() const {
  return ConnectionStateString(state_);
}

std::string Network::GetErrorString() const {
  switch (error_) {
    case ERROR_NO_ERROR:
      // TODO(nkostylev): Introduce new error message "None" instead.
      return std::string();
    case ERROR_OUT_OF_RANGE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
    case ERROR_PIN_MISSING:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
    case ERROR_DHCP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
    case ERROR_CONNECT_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
    case ERROR_BAD_PASSPHRASE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
    case ERROR_BAD_WEPKEY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
    case ERROR_ACTIVATION_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
    case ERROR_NEED_EVDO:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
    case ERROR_NEED_HOME_NETWORK:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
    case ERROR_OTASP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
    case ERROR_AAA_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
    case ERROR_INTERNAL:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
    case ERROR_DNS_LOOKUP_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
    case ERROR_HTTP_GET_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
    case ERROR_IPSEC_PSK_AUTH_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
    case ERROR_IPSEC_CERT_AUTH_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_IPSEC_CERT_AUTH_FAILED);
    case ERROR_PPP_AUTH_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
    case ERROR_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

void Network::SetProxyConfig(const std::string& proxy_config) {
  SetOrClearStringProperty(
      flimflam::kProxyConfigProperty, proxy_config, &proxy_config_);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  if (!EnsureCrosLoaded())
    return;
  // If connected, get IPConfig.
  if (connected() && !device_path_.empty()) {
    CrosListIPConfigs(device_path_,
                      base::Bind(&Network::InitIPAddressCallback,
                                 service_path_));
  }
}

// static
void Network::InitIPAddressCallback(
    const std::string& service_path,
    const NetworkIPConfigVector& ip_configs,
    const std::string& hardware_address) {
  Network* network =
      CrosLibrary::Get()->GetNetworkLibrary()->FindNetworkByPath(service_path);
  if (!network)
    return;
  for (size_t i = 0; i < ip_configs.size(); ++i) {
    const NetworkIPConfig& ipconfig = ip_configs[i];
    if (ipconfig.address.size() > 0) {
      network->ip_address_ = ipconfig.address;
      break;
    }
  }
}

bool Network::UpdateStatus(const std::string& key,
                           const Value& value,
                           PropertyIndex* index) {
  if (network_parser_.get())
    return network_parser_->UpdateStatus(key, value, this, index);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// EthernetNetwork

EthernetNetwork::EthernetNetwork(const std::string& service_path)
    : Network(service_path, TYPE_ETHERNET) {
}

////////////////////////////////////////////////////////////////////////////////
// VirtualNetwork

VirtualNetwork::VirtualNetwork(const std::string& service_path)
    : Network(service_path, TYPE_VPN),
      provider_type_(PROVIDER_TYPE_L2TP_IPSEC_PSK),
      // Assume PSK and user passphrase are not available initially
      psk_passphrase_required_(true),
      user_passphrase_required_(true),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_pointer_factory_(this)) {
}

VirtualNetwork::~VirtualNetwork() {}

void VirtualNetwork::EraseCredentials() {
  WipeString(&ca_cert_nss_);
  WipeString(&psk_passphrase_);
  WipeString(&client_cert_id_);
  WipeString(&user_passphrase_);
}

void VirtualNetwork::CalculateUniqueId() {
  std::string provider_type(ProviderTypeToString(provider_type_));
  set_unique_id(provider_type + "|" + server_hostname_);
}

bool VirtualNetwork::RequiresUserProfile() const {
  return true;
}

void VirtualNetwork::AttemptConnection(const base::Closure& connect) {
  if (client_cert_type() == CLIENT_CERT_TYPE_PATTERN) {
    MatchCertificatePattern(true, connect);
  } else {
    connect.Run();
  }
}

void VirtualNetwork::CopyCredentialsFromRemembered(Network* remembered) {
  CHECK_EQ(remembered->type(), TYPE_VPN);
  VirtualNetwork* remembered_vpn = static_cast<VirtualNetwork*>(remembered);
  VLOG(1) << "Copy VPN credentials: " << name()
          << " username: " << remembered_vpn->username();
  if (ca_cert_nss_.empty())
    ca_cert_nss_ = remembered_vpn->ca_cert_nss();
  if (psk_passphrase_.empty())
    psk_passphrase_ = remembered_vpn->psk_passphrase();
  if (client_cert_id_.empty())
    client_cert_id_ = remembered_vpn->client_cert_id();
  if (username_.empty())
    username_ = remembered_vpn->username();
  if (user_passphrase_.empty())
    user_passphrase_ = remembered_vpn->user_passphrase();
}

bool VirtualNetwork::NeedMoreInfoToConnect() const {
  if (server_hostname_.empty() || username_.empty() ||
      IsUserPassphraseRequired())
    return true;
  if (error() != ERROR_NO_ERROR)
    return true;
  switch (provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      if (IsPSKPassphraseRequired())
        return true;
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      if (client_cert_id_.empty() &&
          client_cert_type() != CLIENT_CERT_TYPE_PATTERN)
        return true;
      break;
    case PROVIDER_TYPE_OPEN_VPN:
      if (client_cert_id_.empty())
        return true;
      // For now we always need additional info for OpenVPN.
      // TODO(stevenjb): Check connectable() once shill sets that state
      // properly, or define another mechanism to determine when additional
      // credentials are required.
      return true;
      break;
    case PROVIDER_TYPE_MAX:
      NOTREACHED();
      break;
  }
  return false;
}

std::string VirtualNetwork::GetProviderTypeString() const {
  switch (provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_PSK);
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_USER_CERT);
      break;
    case PROVIDER_TYPE_OPEN_VPN:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_OPEN_VPN);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
      break;
  }
}

bool VirtualNetwork::IsPSKPassphraseRequired() const {
  return psk_passphrase_required_ && psk_passphrase_.empty();
}

bool VirtualNetwork::IsUserPassphraseRequired() const {
  return user_passphrase_required_ && user_passphrase_.empty();
}

void VirtualNetwork::SetCACertNSS(const std::string& ca_cert_nss) {
  if (provider_type_ == PROVIDER_TYPE_OPEN_VPN) {
    SetStringProperty(
        flimflam::kOpenVPNCaCertNSSProperty, ca_cert_nss, &ca_cert_nss_);
  } else {
    SetStringProperty(
        flimflam::kL2tpIpsecCaCertNssProperty, ca_cert_nss, &ca_cert_nss_);
  }
}

void VirtualNetwork::SetL2TPIPsecPSKCredentials(
    const std::string& psk_passphrase,
    const std::string& username,
    const std::string& user_passphrase,
    const std::string& group_name) {
  if (!psk_passphrase.empty()) {
    SetStringProperty(flimflam::kL2tpIpsecPskProperty,
                      psk_passphrase, &psk_passphrase_);
  }
  SetStringProperty(flimflam::kL2tpIpsecUserProperty, username, &username_);
  if (!user_passphrase.empty()) {
    SetStringProperty(flimflam::kL2tpIpsecPasswordProperty,
                      user_passphrase, &user_passphrase_);
  }
  SetStringProperty(flimflam::kL2tpIpsecGroupNameProperty,
                    group_name, &group_name_);
}

void VirtualNetwork::SetL2TPIPsecCertCredentials(
    const std::string& client_cert_id,
    const std::string& username,
    const std::string& user_passphrase,
    const std::string& group_name) {
  SetStringProperty(flimflam::kL2tpIpsecClientCertIdProperty,
                    client_cert_id, &client_cert_id_);
  SetStringProperty(flimflam::kL2tpIpsecUserProperty, username, &username_);
  if (!user_passphrase.empty()) {
    SetStringProperty(flimflam::kL2tpIpsecPasswordProperty,
                      user_passphrase, &user_passphrase_);
  }
  SetStringProperty(flimflam::kL2tpIpsecGroupNameProperty,
                    group_name, &group_name_);
}

void VirtualNetwork::SetOpenVPNCredentials(
    const std::string& client_cert_id,
    const std::string& username,
    const std::string& user_passphrase,
    const std::string& otp) {
  SetStringProperty(flimflam::kOpenVPNClientCertIdProperty,
                    client_cert_id, &client_cert_id_);
  SetStringProperty(flimflam::kOpenVPNUserProperty, username, &username_);
  if (!user_passphrase.empty()) {
    SetStringProperty(flimflam::kOpenVPNPasswordProperty,
                      user_passphrase, &user_passphrase_);
  }
  SetStringProperty(flimflam::kOpenVPNOTPProperty, otp, NULL);
}

void VirtualNetwork::SetServerHostname(const std::string& server_hostname) {
  SetStringProperty(flimflam::kProviderHostProperty,
                    server_hostname, &server_hostname_);
}

void VirtualNetwork::SetCertificateSlotAndPin(
    const std::string& slot, const std::string& pin) {
  if (provider_type() == PROVIDER_TYPE_OPEN_VPN) {
    SetOrClearStringProperty(flimflam::kOpenVPNClientCertSlotProperty,
                             slot, NULL);
    SetOrClearStringProperty(flimflam::kOpenVPNPinProperty, pin, NULL);
  } else {
    SetOrClearStringProperty(flimflam::kL2tpIpsecClientCertSlotProperty,
                             slot, NULL);
    SetOrClearStringProperty(flimflam::kL2tpIpsecPinProperty, pin, NULL);
  }
}

void VirtualNetwork::MatchCertificatePattern(bool allow_enroll,
                                             const base::Closure& connect) {
  DCHECK(client_cert_type() == CLIENT_CERT_TYPE_PATTERN);
  DCHECK(!client_cert_pattern().Empty());

  // We skip certificate patterns for device policy ONC so that an unmanaged
  // user can't get to the place where a cert is presented for them
  // involuntarily.
  if (client_cert_pattern().Empty() ||
      ui_data().onc_source() == onc::ONC_SOURCE_DEVICE_POLICY) {
    connect.Run();
    return;
  }

  scoped_refptr<net::X509Certificate> matching_cert =
      client_cert_pattern().GetMatch();
  if (matching_cert.get()) {
    std::string client_cert_id =
        x509_certificate_model::GetPkcs11Id(matching_cert->os_cert_handle());
    if (provider_type() == PROVIDER_TYPE_OPEN_VPN) {
      SetStringProperty(flimflam::kOpenVPNClientCertIdProperty,
                        client_cert_id, &client_cert_id_);
    } else {
      SetStringProperty(flimflam::kL2tpIpsecClientCertIdProperty,
                        client_cert_id, &client_cert_id_);
    }
  } else {
    if (allow_enroll && enrollment_delegate()) {
      // Wrap the closure in another callback so that we can retry the
      // certificate match again before actually connecting.
      base::Closure wrapped_connect =
          base::Bind(&VirtualNetwork::MatchCertificatePattern,
                     weak_pointer_factory_.GetWeakPtr(),
                     false,
                     connect);

      enrollment_delegate()->Enroll(client_cert_pattern().enrollment_uri_list(),
                                    wrapped_connect);
      // Enrollment delegate will take care of running the closure at the
      // appropriate time, if the user doesn't cancel.
      return;
    }
  }
  connect.Run();
}

////////////////////////////////////////////////////////////////////////////////
// WirelessNetwork

////////////////////////////////////////////////////////////////////////////////
// CellTower

CellTower::CellTower() {}

////////////////////////////////////////////////////////////////////////////////
// CellularApn

CellularApn::CellularApn() {}

CellularApn::CellularApn(
    const std::string& apn, const std::string& network_id,
    const std::string& username, const std::string& password)
    : apn(apn), network_id(network_id),
      username(username), password(password) {
}

CellularApn::~CellularApn() {}

void CellularApn::Set(const DictionaryValue& dict) {
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnProperty, &apn))
    apn.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnNetworkIdProperty,
                                          &network_id))
    network_id.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnUsernameProperty,
                                          &username))
    username.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnPasswordProperty,
                                          &password))
    password.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnNameProperty, &name))
    name.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnLocalizedNameProperty,
                                          &localized_name))
    localized_name.clear();
  if (!dict.GetStringWithoutPathExpansion(flimflam::kApnLanguageProperty,
                                          &language))
    language.clear();
}

////////////////////////////////////////////////////////////////////////////////
// CellularNetwork

CellularNetwork::CellularNetwork(const std::string& service_path)
    : WirelessNetwork(service_path, TYPE_CELLULAR),
      activate_over_non_cellular_network_(false),
      activation_state_(ACTIVATION_STATE_UNKNOWN),
      network_technology_(NETWORK_TECHNOLOGY_UNKNOWN),
      roaming_state_(ROAMING_STATE_UNKNOWN),
      using_post_(false) {
}

CellularNetwork::~CellularNetwork() {
}

bool CellularNetwork::StartActivation() {
  if (!EnsureCrosLoaded())
    return false;
  if (!CrosActivateCellularModem(service_path(), ""))
    return false;
  // Don't wait for shill to tell us that we are really activating since
  // other notifications in the message loop might cause us to think that
  // the process hasn't started yet.
  activation_state_ = ACTIVATION_STATE_ACTIVATING;
  return true;
}

void CellularNetwork::SetApn(const CellularApn& apn) {
  if (!apn.apn.empty()) {
    DictionaryValue value;
    // Only use the fields that are needed for establishing
    // connections, and ignore the rest.
    value.SetString(flimflam::kApnProperty, apn.apn);
    value.SetString(flimflam::kApnNetworkIdProperty, apn.network_id);
    value.SetString(flimflam::kApnUsernameProperty, apn.username);
    value.SetString(flimflam::kApnPasswordProperty, apn.password);
    SetValueProperty(flimflam::kCellularApnProperty, value);
  } else {
    ClearProperty(flimflam::kCellularApnProperty);
  }
}

bool CellularNetwork::SupportsActivation() const {
  return !usage_url().empty() || !payment_url().empty();
}

bool CellularNetwork::NeedsActivation() const {
  return (activation_state() != ACTIVATION_STATE_ACTIVATED &&
          activation_state() != ACTIVATION_STATE_UNKNOWN);
}

GURL CellularNetwork::GetAccountInfoUrl() const {
  if (!post_data_.length())
    return GURL(payment_url());

  GURL base_url(kRedirectExtensionPage);
  GURL temp_url = chrome_common_net::AppendQueryParameter(base_url,
                                                           "post_data",
                                                           post_data_);
  GURL redir_url = chrome_common_net::AppendQueryParameter(temp_url,
                                                            "formUrl",
                                                            payment_url());
  return redir_url;
}

std::string CellularNetwork::GetNetworkTechnologyString() const {
  // No need to localize these cellular technology abbreviations.
  switch (network_technology_) {
    case NETWORK_TECHNOLOGY_1XRTT:
      return "1xRTT";
      break;
    case NETWORK_TECHNOLOGY_EVDO:
      return "EVDO";
      break;
    case NETWORK_TECHNOLOGY_GPRS:
      return "GPRS";
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      return "EDGE";
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      return "UMTS";
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      return "HSPA";
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      return "HSPA Plus";
      break;
    case NETWORK_TECHNOLOGY_LTE:
      return "LTE";
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      return "LTE Advanced";
      break;
    case NETWORK_TECHNOLOGY_GSM:
      return "GSM";
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_CELLULAR_TECHNOLOGY_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::ActivationStateToString(
    ActivationState activation_state) {
  switch (activation_state) {
    case ACTIVATION_STATE_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATED);
      break;
    case ACTIVATION_STATE_ACTIVATING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATING);
      break;
    case ACTIVATION_STATE_NOT_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_NOT_ACTIVATED);
      break;
    case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_PARTIALLY_ACTIVATED);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetActivationStateString() const {
  return ActivationStateToString(this->activation_state_);
}

std::string CellularNetwork::GetRoamingStateString() const {
  switch (this->roaming_state_) {
    case ROAMING_STATE_HOME:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_HOME);
      break;
    case ROAMING_STATE_ROAMING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_ROAMING);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_UNKNOWN);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WifiNetwork

WifiNetwork::WifiNetwork(const std::string& service_path)
    : WirelessNetwork(service_path, TYPE_WIFI),
      encryption_(SECURITY_NONE),
      passphrase_required_(false),
      hidden_ssid_(false),
      frequency_(0),
      eap_method_(EAP_METHOD_UNKNOWN),
      eap_phase_2_auth_(EAP_PHASE_2_AUTH_AUTO),
      eap_use_system_cas_(true),
      eap_save_credentials_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_pointer_factory_(this)) {
}

WifiNetwork::~WifiNetwork() {}

void WifiNetwork::CalculateUniqueId() {
  ConnectionSecurity encryption = encryption_;
  // Shill treats wpa and rsn as psk internally, so convert those types
  // to psk for unique naming.
  if (encryption == SECURITY_WPA || encryption == SECURITY_RSN)
    encryption = SECURITY_PSK;
  std::string security = std::string(SecurityToString(encryption));
  set_unique_id(security + "|" + name());
}

bool WifiNetwork::SetSsid(const std::string& ssid) {
  // Detects encoding and convert to UTF-8.
  std::string ssid_utf8;
  if (!IsStringUTF8(ssid)) {
    std::string encoding;
    if (base::DetectEncoding(ssid, &encoding)) {
      if (!base::ConvertToUtf8AndNormalize(ssid, encoding, &ssid_utf8)) {
        ssid_utf8.clear();
      }
    }
  }

  if (ssid_utf8.empty())
    SetName(ssid);
  else
    SetName(ssid_utf8);

  return true;
}

bool WifiNetwork::SetHexSsid(const std::string& ssid_hex) {
  // Converts ascii hex dump (eg. "49656c6c6f") to string (eg. "Hello").
  std::vector<uint8> ssid_raw;
  if (!base::HexStringToBytes(ssid_hex, &ssid_raw)) {
    LOG(ERROR) << "Illegal hex char is found in WiFi.HexSSID.";
    ssid_raw.clear();
    return false;
  }

  return SetSsid(std::string(ssid_raw.begin(), ssid_raw.end()));
}

const std::string& WifiNetwork::GetPassphrase() const {
  if (!user_passphrase_.empty())
    return user_passphrase_;
  return passphrase_;
}

void WifiNetwork::SetPassphrase(const std::string& passphrase) {
  // Set the user_passphrase_ only; passphrase_ stores the shill value.
  // If the user sets an empty passphrase, restore it to the passphrase
  // remembered by shill.
  if (!passphrase.empty()) {
    user_passphrase_ = passphrase;
    passphrase_ = passphrase;
  } else {
    user_passphrase_ = passphrase_;
  }
  // Send the change to shill. If the format is valid, it will propagate to
  // passphrase_ with a service update.
  SetOrClearStringProperty(flimflam::kPassphraseProperty, passphrase, NULL);
}

// See src/third_party/shill/doc/service-api.txt for properties that
// shill will forget when SaveCredentials is false.
void WifiNetwork::EraseCredentials() {
  WipeString(&passphrase_);
  WipeString(&user_passphrase_);
  WipeString(&eap_client_cert_pkcs11_id_);
  WipeString(&eap_identity_);
  WipeString(&eap_anonymous_identity_);
  WipeString(&eap_passphrase_);
}

void WifiNetwork::SetIdentity(const std::string& identity) {
  SetStringProperty(flimflam::kIdentityProperty, identity, &identity_);
}

void WifiNetwork::SetHiddenSSID(bool hidden_ssid) {
  SetBooleanProperty(flimflam::kWifiHiddenSsid, hidden_ssid, &hidden_ssid_);
}

void WifiNetwork::SetEAPMethod(EAPMethod method) {
  eap_method_ = method;
  switch (method) {
    case EAP_METHOD_PEAP:
      SetStringProperty(
          flimflam::kEapMethodProperty, flimflam::kEapMethodPEAP, NULL);
      break;
    case EAP_METHOD_TLS:
      SetStringProperty(
          flimflam::kEapMethodProperty, flimflam::kEapMethodTLS, NULL);
      break;
    case EAP_METHOD_TTLS:
      SetStringProperty(
          flimflam::kEapMethodProperty, flimflam::kEapMethodTTLS, NULL);
      break;
    case EAP_METHOD_LEAP:
      SetStringProperty(
          flimflam::kEapMethodProperty, flimflam::kEapMethodLEAP, NULL);
      break;
    default:
      ClearProperty(flimflam::kEapMethodProperty);
      break;
  }
}

void WifiNetwork::SetEAPPhase2Auth(EAPPhase2Auth auth) {
  eap_phase_2_auth_ = auth;
  bool is_peap = (eap_method_ == EAP_METHOD_PEAP);
  switch (auth) {
    case EAP_PHASE_2_AUTH_AUTO:
      ClearProperty(flimflam::kEapPhase2AuthProperty);
      break;
    case EAP_PHASE_2_AUTH_MD5:
      SetStringProperty(flimflam::kEapPhase2AuthProperty,
                        is_peap ? flimflam::kEapPhase2AuthPEAPMD5
                                : flimflam::kEapPhase2AuthTTLSMD5,
                        NULL);
      break;
    case EAP_PHASE_2_AUTH_MSCHAPV2:
      SetStringProperty(flimflam::kEapPhase2AuthProperty,
                        is_peap ? flimflam::kEapPhase2AuthPEAPMSCHAPV2
                                : flimflam::kEapPhase2AuthTTLSMSCHAPV2,
                        NULL);
      break;
    case EAP_PHASE_2_AUTH_MSCHAP:
      SetStringProperty(flimflam::kEapPhase2AuthProperty,
                        flimflam::kEapPhase2AuthTTLSMSCHAP, NULL);
      break;
    case EAP_PHASE_2_AUTH_PAP:
      SetStringProperty(flimflam::kEapPhase2AuthProperty,
                        flimflam::kEapPhase2AuthTTLSPAP, NULL);
      break;
    case EAP_PHASE_2_AUTH_CHAP:
      SetStringProperty(flimflam::kEapPhase2AuthProperty,
                        flimflam::kEapPhase2AuthTTLSCHAP, NULL);
      break;
  }
}

void WifiNetwork::SetEAPServerCaCertNssNickname(
    const std::string& nss_nickname) {
  VLOG(1) << "SetEAPServerCaCertNssNickname " << nss_nickname;
  SetOrClearStringProperty(flimflam::kEapCaCertNssProperty,
                           nss_nickname, &eap_server_ca_cert_nss_nickname_);
}

void WifiNetwork::SetEAPClientCertPkcs11Id(const std::string& pkcs11_id) {
  VLOG(1) << "SetEAPClientCertPkcs11Id " << pkcs11_id;
  SetOrClearStringProperty(
      flimflam::kEapCertIdProperty, pkcs11_id, &eap_client_cert_pkcs11_id_);
  // shill requires both CertID and KeyID for TLS connections, despite
  // the fact that by convention they are the same ID.
  SetOrClearStringProperty(flimflam::kEapKeyIdProperty, pkcs11_id, NULL);
}

void WifiNetwork::SetEAPUseSystemCAs(bool use_system_cas) {
  SetBooleanProperty(flimflam::kEapUseSystemCasProperty, use_system_cas,
                     &eap_use_system_cas_);
}

void WifiNetwork::SetEAPIdentity(const std::string& identity) {
  SetOrClearStringProperty(
      flimflam::kEapIdentityProperty, identity, &eap_identity_);
}

void WifiNetwork::SetEAPAnonymousIdentity(const std::string& identity) {
  SetOrClearStringProperty(flimflam::kEapAnonymousIdentityProperty, identity,
                           &eap_anonymous_identity_);
}

void WifiNetwork::SetEAPPassphrase(const std::string& passphrase) {
  SetOrClearStringProperty(
      flimflam::kEapPasswordProperty, passphrase, &eap_passphrase_);
}

std::string WifiNetwork::GetEncryptionString() const {
  switch (encryption_) {
    case SECURITY_UNKNOWN:
      break;
    case SECURITY_NONE:
      return "";
    case SECURITY_WEP:
      return "WEP";
    case SECURITY_WPA:
      return "WPA";
    case SECURITY_RSN:
      return "RSN";
    case SECURITY_8021X: {
      std::string result("8021X");
      switch (eap_method_) {
        case EAP_METHOD_PEAP:
          result += "+PEAP";
          break;
        case EAP_METHOD_TLS:
          result += "+TLS";
          break;
        case EAP_METHOD_TTLS:
          result += "+TTLS";
          break;
        case EAP_METHOD_LEAP:
          result += "+LEAP";
          break;
        default:
          break;
      }
      return result;
    }
    case SECURITY_PSK:
      return "PSK";
  }
  return "Unknown";
}

bool WifiNetwork::IsPassphraseRequired() const {
  if (encryption_ == SECURITY_NONE)
    return false;
  // A connection failure might be due to a bad passphrase.
  if (error() == ERROR_BAD_PASSPHRASE || error() == ERROR_BAD_WEPKEY ||
      error() == ERROR_UNKNOWN)
    return true;
  // For 802.1x networks, configuration is required if connectable is false
  // unless we're using a certificate pattern.
  if (encryption_ == SECURITY_8021X) {
    if (eap_method_ != EAP_METHOD_TLS ||
        client_cert_type() != CLIENT_CERT_TYPE_PATTERN)
      return !connectable();
    return false;
  }
  return passphrase_required_;
}

bool WifiNetwork::RequiresUserProfile() const {
  // 8021X requires certificates which are only stored for individual users.
  if (encryption_ != SECURITY_8021X)
    return false;

  if (eap_method_ != EAP_METHOD_TLS)
    return false;

  if (eap_client_cert_pkcs11_id().empty() &&
      client_cert_type() != CLIENT_CERT_TYPE_PATTERN)
        return false;

  return true;
}

void WifiNetwork::AttemptConnection(const base::Closure& connect) {
  if (client_cert_type() == CLIENT_CERT_TYPE_PATTERN) {
    MatchCertificatePattern(true, connect);
  } else {
    connect.Run();
  }
}

void WifiNetwork::SetCertificatePin(const std::string& pin) {
  SetOrClearStringProperty(flimflam::kEapPinProperty, pin, NULL);
}

void WifiNetwork::MatchCertificatePattern(bool allow_enroll,
                                          const base::Closure& connect) {
  DCHECK(client_cert_type() == CLIENT_CERT_TYPE_PATTERN);
  DCHECK(!client_cert_pattern().Empty());
  if (client_cert_pattern().Empty()) {
    connect.Run();
    return;
  }

  scoped_refptr<net::X509Certificate> matching_cert =
      client_cert_pattern().GetMatch();
  if (matching_cert.get()) {
    SetEAPClientCertPkcs11Id(
        x509_certificate_model::GetPkcs11Id(matching_cert->os_cert_handle()));
  } else {
    if (allow_enroll && enrollment_delegate()) {
      // Wrap the closure in another callback so that we can retry the
      // certificate match again before actually connecting.
      base::Closure wrapped_connect =
          base::Bind(&WifiNetwork::MatchCertificatePattern,
                     weak_pointer_factory_.GetWeakPtr(),
                     false,
                     connect);

      enrollment_delegate()->Enroll(client_cert_pattern().enrollment_uri_list(),
                                    wrapped_connect);
      // Enrollment delegate should take care of running the closure at the
      // appropriate time, if the user doesn't cancel.
      return;
    }
  }
  connect.Run();
}

////////////////////////////////////////////////////////////////////////////////
// WimaxNetwork

WimaxNetwork::WimaxNetwork(const std::string& service_path)
    : WirelessNetwork(service_path, TYPE_WIMAX),
      passphrase_required_(false) {
}

WimaxNetwork::~WimaxNetwork() {
}

void WimaxNetwork::EraseCredentials() {
  WipeString(&eap_passphrase_);
  WipeString(&eap_identity_);
}

void WimaxNetwork::SetEAPPassphrase(const std::string& passphrase) {
  SetOrClearStringProperty(
      flimflam::kEapPasswordProperty, passphrase, &eap_passphrase_);
}

void WimaxNetwork::SetEAPIdentity(const std::string& identity) {
  SetOrClearStringProperty(
      flimflam::kEapIdentityProperty, identity, &eap_identity_);
}

void WimaxNetwork::CalculateUniqueId() {
  set_unique_id(name() + "|" + eap_identity());
}

NetworkLibrary::EAPConfigData::EAPConfigData()
    : method(EAP_METHOD_UNKNOWN),
      auth(EAP_PHASE_2_AUTH_AUTO),
      use_system_cas(true) {
}

NetworkLibrary::EAPConfigData::~EAPConfigData() {}

NetworkLibrary::VPNConfigData::VPNConfigData()
    : save_credentials(false) {
}

NetworkLibrary::VPNConfigData::~VPNConfigData() {}

// static
NetworkLibrary* NetworkLibrary::GetImpl(bool stub) {
  NetworkLibrary* impl;
  if (stub)
    impl = new NetworkLibraryImplStub();
  else
    impl = new NetworkLibraryImplCros();
  impl->Init();
  return impl;
}

}  // namespace chromeos
