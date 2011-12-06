// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <dbus/dbus-glib.h>
#include <dbus/dbus-gtype-specialized.h>
#include <glib-object.h>

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"  // for debug output only.
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversion_utils.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/native_network_parser.h"
#include "chrome/browser/chromeos/cros/network_ui_data.h"
#include "chrome/browser/chromeos/cros/onc_network_parser.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/network_login_observer.h"
#include "chrome/common/time_format.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/nss_util.h"  // crypto::GetTPMTokenInfo() for 802.1X and VPN.
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

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

// Only send network change notifications to observers once every 50ms.
const int kNetworkNotifyDelayMs = 50;

// How long we should remember that cellular plan payment was received.
const int kRecentPlanPaymentHours = 6;

// Default value of the SIM unlock retries count. It is updated to the real
// retries count once cellular device with SIM card is initialized.
// If cellular device doesn't have SIM card, then retries are never used.
const int kDefaultSimUnlockRetriesCount = 999;

// List of cellular operators names that should have data roaming always enabled
// to be able to connect to any network.
const char* kAlwaysInRoamingOperators[] = {
  "CUBIC"
};

////////////////////////////////////////////////////////////////////////////////
// Misc.

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

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

////////////////////////////////////////////////////////////////////////////////
// glib

Value* ConvertGlibValue(const GValue* gvalue);

void AppendListElement(const GValue* gvalue, gpointer user_data) {
  ListValue* list = static_cast<ListValue*>(user_data);
  Value* value = ConvertGlibValue(gvalue);
  list->Append(value);
}

void AppendDictionaryElement(const GValue* keyvalue,
                             const GValue* gvalue,
                             gpointer user_data) {
  DictionaryValue* dict = static_cast<DictionaryValue*>(user_data);
  std::string key(g_value_get_string(keyvalue));
  Value* value = ConvertGlibValue(gvalue);
  dict->SetWithoutPathExpansion(key, value);
}

Value* ConvertGlibValue(const GValue* gvalue) {
  if (G_VALUE_HOLDS_STRING(gvalue)) {
    return Value::CreateStringValue(g_value_get_string(gvalue));
  } else if (G_VALUE_HOLDS_BOOLEAN(gvalue)) {
    return Value::CreateBooleanValue(
        static_cast<bool>(g_value_get_boolean(gvalue)));
  } else if (G_VALUE_HOLDS_INT(gvalue)) {
    return Value::CreateIntegerValue(g_value_get_int(gvalue));
  } else if (G_VALUE_HOLDS_UINT(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uint(gvalue)));
  } else if (G_VALUE_HOLDS_UCHAR(gvalue)) {
    return Value::CreateIntegerValue(
        static_cast<int>(g_value_get_uchar(gvalue)));
  } else if (G_VALUE_HOLDS(gvalue, DBUS_TYPE_G_OBJECT_PATH)) {
    const char* path = static_cast<const char*>(g_value_get_boxed(gvalue));
    return Value::CreateStringValue(path);
  } else if (G_VALUE_HOLDS(gvalue, G_TYPE_STRV)) {
    ListValue* list = new ListValue();
    for (GStrv strv = static_cast<GStrv>(g_value_get_boxed(gvalue));
         *strv != NULL; ++strv) {
      list->Append(Value::CreateStringValue(*strv));
    }
    return list;
  } else if (dbus_g_type_is_collection(G_VALUE_TYPE(gvalue))) {
    ListValue* list = new ListValue();
    dbus_g_type_collection_value_iterate(gvalue, AppendListElement, list);
    return list;
  } else if (dbus_g_type_is_map(G_VALUE_TYPE(gvalue))) {
    DictionaryValue* dict = new DictionaryValue();
    dbus_g_type_map_value_iterate(gvalue, AppendDictionaryElement, dict);
    return dict;
  } else if (G_VALUE_HOLDS(gvalue, G_TYPE_VALUE)) {
    const GValue* bvalue = static_cast<GValue*>(g_value_get_boxed(gvalue));
    return ConvertGlibValue(bvalue);
  } else {
    LOG(ERROR) << "Unrecognized Glib value type: " << G_VALUE_TYPE(gvalue);
    return Value::CreateNullValue();
  }
}

DictionaryValue* ConvertGHashTable(GHashTable* ghash) {
  DictionaryValue* dict = new DictionaryValue();
  GHashTableIter iter;
  gpointer gkey, gvalue;
  g_hash_table_iter_init(&iter, ghash);
  while (g_hash_table_iter_next(&iter, &gkey, &gvalue))  {
    std::string key(static_cast<char*>(gkey));
    Value* value = ConvertGlibValue(static_cast<GValue*>(gvalue));
    dict->SetWithoutPathExpansion(key, value);
  }
  return dict;
}

GValue* ConvertBoolToGValue(bool b) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_BOOLEAN);
  g_value_set_boolean(gvalue, b);
  return gvalue;
}

GValue* ConvertIntToGValue(int i) {
  // Converting to a 32-bit signed int type in particular, since
  // that's what flimflam expects in its DBus API
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_INT);
  g_value_set_int(gvalue, i);
  return gvalue;
}

GValue* ConvertStringToGValue(const std::string& s) {
  GValue* gvalue = new GValue();
  g_value_init(gvalue, G_TYPE_STRING);
  g_value_set_string(gvalue, s.c_str());
  return gvalue;
}

GValue* ConvertDictionaryValueToGValue(const DictionaryValue* dict) {
  GHashTable* ghash =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  for (DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    std::string key = *it;
    std::string val;
    if (!dict->GetString(key, &val)) {
      NOTREACHED() << "Invalid type in dictionary, key: " << key;
      continue;
    }
    g_hash_table_insert(ghash,
                        g_strdup(const_cast<char*>(key.c_str())),
                        g_strdup(const_cast<char*>(val.c_str())));
  }
  GValue* gvalue = new GValue();
  g_value_init(gvalue, DBUS_TYPE_G_STRING_STRING_HASHTABLE);
  g_value_set_boxed(gvalue, ghash);
  return gvalue;
}

GValue* ConvertValueToGValue(const Value* value) {
  switch (value->GetType()) {
    case Value::TYPE_BOOLEAN: {
      bool out;
      if (value->GetAsBoolean(&out))
        return ConvertBoolToGValue(out);
      break;
    }
    case Value::TYPE_INTEGER: {
      int out;
      if (value->GetAsInteger(&out))
        return ConvertIntToGValue(out);
      break;
    }
    case Value::TYPE_STRING: {
      std::string out;
      if (value->GetAsString(&out))
        return ConvertStringToGValue(out);
      break;
    }
    case Value::TYPE_DICTIONARY: {
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
      return ConvertDictionaryValueToGValue(dict);
    }
    case Value::TYPE_NULL:
    case Value::TYPE_DOUBLE:
    case Value::TYPE_BINARY:
    case Value::TYPE_LIST:
      // Other Value types shouldn't be passed through this mechanism.
      NOTREACHED() << "Unconverted Value of type: " << value->GetType();
      return new GValue();
  }
  NOTREACHED() << "Value conversion failed, type: " << value->GetType();
  return new GValue();
}

GHashTable* ConvertDictionaryValueToGValueMap(const DictionaryValue* dict) {
  GHashTable* ghash =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  for (DictionaryValue::key_iterator it = dict->begin_keys();
       it != dict->end_keys(); ++it) {
    std::string key = *it;
    Value* val = NULL;
    if (dict->Get(key, &val)) {
      g_hash_table_insert(ghash,
                          g_strdup(const_cast<char*>(key.c_str())),
                          ConvertValueToGValue(val));
    }
  }
  return ghash;
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

void Network::SetNetworkParser(NetworkParser* parser) {
  network_parser_.reset(parser);
}

void Network::UpdatePropertyMap(PropertyIndex index, const base::Value& value) {
  // Add the property to property_map_.  Delete previous value if necessary.
  Value*& entry = property_map_[index];
  delete entry;
  entry = value.DeepCopy();
  if (VLOG_IS_ON(2)) {
    std::string value_json;
    base::JSONWriter::Write(&value, true, &value_json);
    VLOG(2) << "Updated property map on network: "
            << unique_id() << "[" << index << "] = " << value_json;
  }
}

void Network::SetState(ConnectionState new_state) {
  if (new_state == state_)
    return;
  ConnectionState old_state = state_;
  state_ = new_state;
  if (!IsConnectingState(new_state))
    set_connection_started(false);
  if (new_state == STATE_FAILURE) {
    if (old_state != STATE_UNKNOWN &&
        old_state != STATE_IDLE) {
      // New failure, the user needs to be notified.
      // Transition STATE_IDLE -> STATE_FAILURE sometimes happens on resume
      // but is not an actual failure as network device is not ready yet.
      notify_failure_ = true;
      // Normally error_ should be set, but if it is not we need to set it to
      // something here so that the retry logic will be triggered.
      if (error_ == ERROR_NO_ERROR)
        error_ = ERROR_UNKNOWN;
    }
  } else {
    // State changed, so refresh IP address.
    // Note: blocking DBus call. TODO(stevenjb): refactor this.
    InitIPAddress();
  }
  VLOG(1) << name() << ".State = " << GetStateString();
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

void Network::SetValueProperty(const char* prop, Value* value) {
  DCHECK(prop);
  DCHECK(value);
  if (!EnsureCrosLoaded())
    return;
  scoped_ptr<GValue> gvalue(ConvertValueToGValue(value));
  chromeos::SetNetworkServicePropertyGValue(
      service_path_.c_str(), prop, gvalue.get());
}

void Network::ClearProperty(const char* prop) {
  DCHECK(prop);
  if (!EnsureCrosLoaded())
    return;
  chromeos::ClearNetworkServiceProperty(service_path_.c_str(), prop);
}

void Network::SetStringProperty(
    const char* prop, const std::string& str, std::string* dest) {
  if (dest)
    *dest = str;
  scoped_ptr<Value> value(Value::CreateStringValue(str));
  SetValueProperty(prop, value.get());
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
  scoped_ptr<Value> value(Value::CreateBooleanValue(b));
  SetValueProperty(prop, value.get());
}

void Network::SetIntegerProperty(const char* prop, int i, int* dest) {
  if (dest)
    *dest = i;
  scoped_ptr<Value> value(Value::CreateIntegerValue(i));
  SetValueProperty(prop, value.get());
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

void Network::SetProfilePath(const std::string& profile_path) {
  VLOG(1) << "Setting profile for: " << name_ << " to: " << profile_path;
  SetOrClearStringProperty(
      flimflam::kProfileProperty, profile_path, &profile_path_);
}

std::string Network::GetStateString() const {
  switch (state_) {
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
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
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
  SetStringProperty(
      flimflam::kProxyConfigProperty, proxy_config, &proxy_config_);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  if (!EnsureCrosLoaded())
    return;
  // If connected, get ip config.
  if (connected() && !device_path_.empty()) {
    IPConfigStatus* ipconfig_status =
        chromeos::ListIPConfigs(device_path_.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; ++i) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        if (strlen(ipconfig.address) > 0) {
          ip_address_ = ipconfig.address;
          break;
        }
      }
      chromeos::FreeIPConfigStatus(ipconfig_status);
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
      provider_type_(PROVIDER_TYPE_L2TP_IPSEC_PSK) {
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

void VirtualNetwork::CopyCredentialsFromRemembered(Network* remembered) {
  DCHECK_EQ(remembered->type(), TYPE_VPN);
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
  if (server_hostname_.empty() || username_.empty() || user_passphrase_.empty())
    return true;
  if (error() != ERROR_NO_ERROR)
    return true;
  switch (provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      if (psk_passphrase_.empty())
        return true;
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      if (client_cert_id_.empty())
        return true;
      break;
    case PROVIDER_TYPE_OPEN_VPN:
      if (client_cert_id_.empty())
        return true;
      // For now we always need additional info for OpenVPN.
      // TODO(stevenjb): Check connectable() once flimflam sets that state
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
  switch (this->provider_type_) {
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

void VirtualNetwork::SetCACertNSS(const std::string& ca_cert_nss) {
  SetStringProperty(
      flimflam::kL2tpIpsecCaCertNssProperty, ca_cert_nss, &ca_cert_nss_);
}

void VirtualNetwork::SetL2TPIPsecPSKCredentials(
    const std::string& psk_passphrase,
    const std::string& username,
    const std::string& user_passphrase,
    const std::string& group_name) {
  SetStringProperty(flimflam::kL2tpIpsecPskProperty,
                    psk_passphrase, &psk_passphrase_);
  SetStringProperty(flimflam::kL2tpIpsecUserProperty, username, &username_);
  SetStringProperty(flimflam::kL2tpIpsecPasswordProperty,
                    user_passphrase, &user_passphrase_);
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
  SetStringProperty(flimflam::kL2tpIpsecPasswordProperty,
                    user_passphrase, &user_passphrase_);
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
  SetStringProperty(flimflam::kOpenVPNPasswordProperty,
                    user_passphrase, &user_passphrase_);
  SetStringProperty(flimflam::kOpenVPNOTPProperty, otp, NULL);
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

////////////////////////////////////////////////////////////////////////////////
// WirelessNetwork

////////////////////////////////////////////////////////////////////////////////
// CellularDataPlan

CellularDataPlan::CellularDataPlan()
    : plan_name("Unknown"),
      plan_type(CELLULAR_DATA_PLAN_UNLIMITED),
      plan_data_bytes(0),
      data_bytes_used(0) {
}

CellularDataPlan::CellularDataPlan(const CellularDataPlanInfo &plan)
    : plan_name(plan.plan_name ? plan.plan_name : ""),
      plan_type(plan.plan_type),
      update_time(base::Time::FromInternalValue(plan.update_time)),
      plan_start_time(base::Time::FromInternalValue(plan.plan_start_time)),
      plan_end_time(base::Time::FromInternalValue(plan.plan_end_time)),
      plan_data_bytes(plan.plan_data_bytes),
      data_bytes_used(plan.data_bytes_used) {
}

CellularDataPlan::~CellularDataPlan() {}

string16 CellularDataPlan::GetPlanDesciption() const {
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_UNLIMITED_DATA,
          base::TimeFormatFriendlyDate(plan_start_time));
      break;
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_DATA,
                ui::FormatBytes(plan_data_bytes),
                base::TimeFormatFriendlyDate(plan_start_time));
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
                ui::FormatBytes(plan_data_bytes),
                base::TimeFormatFriendlyDate(plan_start_time));
    default:
      break;
    }
  }
  return string16();
}

string16 CellularDataPlan::GetRemainingWarning() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    if (remaining_time().InSeconds() <= chromeos::kCellularDataVeryLowSecs) {
      return GetPlanExpiration();
    }
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    if (remaining_data() <= chromeos::kCellularDataVeryLowBytes) {
      int64 remaining_mbytes = remaining_data() / (1024 * 1024);
      return l10n_util::GetStringFUTF16(
          IDS_NETWORK_DATA_REMAINING_MESSAGE,
          UTF8ToUTF16(base::Int64ToString(remaining_mbytes)));
    }
  }
  return string16();
}

string16 CellularDataPlan::GetDataRemainingDesciption() const {
  int64 remaining_bytes = remaining_data();
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_UNLIMITED);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return ui::FormatBytes(remaining_bytes);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return ui::FormatBytes(remaining_bytes);
    }
    default:
      break;
  }
  return string16();
}

string16 CellularDataPlan::GetUsageInfo() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    return GetPlanExpiration();
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    int64 remaining_bytes = remaining_data();
    if (remaining_bytes == 0) {
      return l10n_util::GetStringUTF16(
          IDS_NETWORK_DATA_NONE_AVAILABLE_MESSAGE);
    } else if (remaining_bytes < 1024 * 1024) {
      return l10n_util::GetStringUTF16(
          IDS_NETWORK_DATA_LESS_THAN_ONE_MB_AVAILABLE_MESSAGE);
    } else {
      int64 remaining_mb = remaining_bytes / (1024 * 1024);
      return l10n_util::GetStringFUTF16(
          IDS_NETWORK_DATA_MB_AVAILABLE_MESSAGE,
          UTF8ToUTF16(base::Int64ToString(remaining_mb)));
    }
  }
  return string16();
}

std::string CellularDataPlan::GetUniqueIdentifier() const {
  // A cellular plan is uniquely described by the union of name, type,
  // start time, end time, and max bytes.
  // So we just return a union of all these variables.
  return plan_name + "|" +
      base::Int64ToString(plan_type) + "|" +
      base::Int64ToString(plan_start_time.ToInternalValue()) + "|" +
      base::Int64ToString(plan_end_time.ToInternalValue()) + "|" +
      base::Int64ToString(plan_data_bytes);
}

base::TimeDelta CellularDataPlan::remaining_time() const {
  base::TimeDelta time = plan_end_time - base::Time::Now();
  return time.InMicroseconds() < 0 ? base::TimeDelta() : time;
}

int64 CellularDataPlan::remaining_minutes() const {
  return remaining_time().InMinutes();
}

int64 CellularDataPlan::remaining_data() const {
  int64 data = plan_data_bytes - data_bytes_used;
  return data < 0 ? 0 : data;
}

string16 CellularDataPlan::GetPlanExpiration() const {
  return TimeFormat::TimeRemaining(remaining_time());
}

////////////////////////////////////////////////////////////////////////////////
// CellTower

CellTower::CellTower() {}

////////////////////////////////////////////////////////////////////////////////
// WifiAccessPoint

WifiAccessPoint::WifiAccessPoint() {}

////////////////////////////////////////////////////////////////////////////////
// NetworkIPConfig

NetworkIPConfig::NetworkIPConfig(
    const std::string& device_path, IPConfigType type,
    const std::string& address, const std::string& netmask,
    const std::string& gateway, const std::string& name_servers)
    : device_path(device_path),
      type(type),
      address(address),
      netmask(netmask),
      gateway(gateway),
      name_servers(name_servers) {
}

NetworkIPConfig::~NetworkIPConfig() {}

int32 NetworkIPConfig::GetPrefixLength() const {
  int count = 0;
  int prefixlen = 0;
  StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, then it's invalid.
    if (count == 4) {
      return -1;
    }
    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefixlen / 8 != count) {
      if (token != "0") {
        return -1;
      }
    } else if (token == "255") {
      prefixlen += 8;
    } else if (token == "254") {
      prefixlen += 7;
    } else if (token == "252") {
      prefixlen += 6;
    } else if (token == "248") {
      prefixlen += 5;
    } else if (token == "240") {
      prefixlen += 4;
    } else if (token == "224") {
      prefixlen += 3;
    } else if (token == "192") {
      prefixlen += 2;
    } else if (token == "128") {
      prefixlen += 1;
    } else if (token == "0") {
      prefixlen += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefixlen;
}

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
      activation_state_(ACTIVATION_STATE_UNKNOWN),
      network_technology_(NETWORK_TECHNOLOGY_UNKNOWN),
      roaming_state_(ROAMING_STATE_UNKNOWN),
      using_post_(false),
      data_left_(DATA_UNKNOWN) {
}

CellularNetwork::~CellularNetwork() {
}

bool CellularNetwork::StartActivation() {
  if (!EnsureCrosLoaded())
    return false;
  if (!chromeos::ActivateCellularModem(service_path().c_str(), NULL))
    return false;
  // Don't wait for flimflam to tell us that we are really activating since
  // other notifications in the message loop might cause us to think that
  // the process hasn't started yet.
  activation_state_ = ACTIVATION_STATE_ACTIVATING;
  return true;
}

void CellularNetwork::RefreshDataPlansIfNeeded() const {
  if (!EnsureCrosLoaded())
    return;
  if (connected() && activated())
    chromeos::RequestCellularDataPlanUpdate(service_path().c_str());
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
    SetValueProperty(flimflam::kCellularApnProperty, &value);
  } else {
    ClearProperty(flimflam::kCellularApnProperty);
  }
}

bool CellularNetwork::SupportsActivation() const {
  return SupportsDataPlan();
}

bool CellularNetwork::SupportsDataPlan() const {
  // TODO(nkostylev): Are there cases when only one of this is defined?
  return !usage_url().empty() || !payment_url().empty();
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
      eap_method_(EAP_METHOD_UNKNOWN),
      eap_phase_2_auth_(EAP_PHASE_2_AUTH_AUTO),
      eap_use_system_cas_(true) {
}

WifiNetwork::~WifiNetwork() {}

void WifiNetwork::CalculateUniqueId() {
  ConnectionSecurity encryption = encryption_;
  // Flimflam treats wpa and rsn as psk internally, so convert those types
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
  // Set the user_passphrase_ only; passphrase_ stores the flimflam value.
  // If the user sets an empty passphrase, restore it to the passphrase
  // remembered by flimflam.
  if (!passphrase.empty()) {
    user_passphrase_ = passphrase;
    passphrase_ = passphrase;
  } else {
    user_passphrase_ = passphrase_;
  }
  // Send the change to flimflam. If the format is valid, it will propagate to
  // passphrase_ with a service update.
  SetOrClearStringProperty(flimflam::kPassphraseProperty, passphrase, NULL);
}

// See src/third_party/flimflam/doc/service-api.txt for properties that
// flimflam will forget when SaveCredentials is false.
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
  // flimflam requires both CertID and KeyID for TLS connections, despite
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
  // TODO(stevenjb): Remove error_ tests when fixed in flimflam
  // (http://crosbug.com/10135).
  if (error() == ERROR_BAD_PASSPHRASE || error() == ERROR_BAD_WEPKEY)
    return true;
  // For 802.1x networks, configuration is required if connectable is false.
  if (encryption_ == SECURITY_8021X)
    return !connectable();
  return passphrase_required_;
}

bool WifiNetwork::RequiresUserProfile() const {
  // 8021X requires certificates which are only stored for individual users.
  if (encryption_ == SECURITY_8021X &&
      (eap_method_ == EAP_METHOD_TLS ||
       !eap_client_cert_pkcs11_id().empty()))
    return true;
  return false;
}

void WifiNetwork::SetCertificatePin(const std::string& pin) {
  SetOrClearStringProperty(flimflam::kEapPinProperty, pin, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

class NetworkLibraryImplBase : public NetworkLibrary  {
 public:
  NetworkLibraryImplBase();
  virtual ~NetworkLibraryImplBase();

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase virtual functions.

  // Functions for monitoring networks & devices.
  virtual void MonitorNetworkStart(const std::string& service_path) = 0;
  virtual void MonitorNetworkStop(const std::string& service_path) = 0;
  virtual void MonitorNetworkDeviceStart(const std::string& device_path) = 0;
  virtual void MonitorNetworkDeviceStop(const std::string& device_path) = 0;

  // Called from ConnectToWifiNetwork.
  // Calls ConnectToWifiNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) = 0;
  // Called from ConnectToVirtualNetwork*.
  // Calls ConnectToVirtualNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type) = 0;
  // Call to configure a wifi service. The identifier is either a service_path
  // or a GUID. |info| is a dictionary of property values.
  virtual void CallConfigureService(const std::string& identifier,
                                    const DictionaryValue* info) = 0;
  // Called from NetworkConnectStart.
  // Calls NetworkConnectCompleted when the connection attept completes.
  virtual void CallConnectToNetwork(Network* network) = 0;
  // Called from DeleteRememberedNetwork.
  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path, const std::string& service_path) = 0;

  // Called from Enable*NetworkDevice.
  // Asynchronously enables or disables the specified device type.
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) = 0;

  // Called from DeleteRememberedNetwork for VPN services.
  // Asynchronously disconnects and removes the service.
  virtual void CallRemoveNetwork(const Network* network) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibrary implementation.

  // virtual Init implemented in derived classes.
  // virtual IsCros implemented in derived classes.

  virtual void AddNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void RemoveNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) OVERRIDE;
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) OVERRIDE;
  virtual void RemoveObserverForAllNetworks(
      NetworkObserver* observer) OVERRIDE;
  virtual void AddNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;
  virtual void RemoveNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;

  virtual void Lock() OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual bool IsLocked() OVERRIDE;

  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void AddPinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void RemovePinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void AddUserActionObserver(
      UserActionObserver* observer) OVERRIDE;
  virtual void RemoveUserActionObserver(
      UserActionObserver* observer) OVERRIDE;

  virtual const EthernetNetwork* ethernet_network() const OVERRIDE {
    return ethernet_;
  }
  virtual bool ethernet_connecting() const OVERRIDE {
    return ethernet_ ? ethernet_->connecting() : false;
  }
  virtual bool ethernet_connected() const OVERRIDE {
    return ethernet_ ? ethernet_->connected() : false;
  }
  virtual const WifiNetwork* wifi_network() const OVERRIDE {
    return active_wifi_;
  }
  virtual bool wifi_connecting() const OVERRIDE {
    return active_wifi_ ? active_wifi_->connecting() : false;
  }
  virtual bool wifi_connected() const OVERRIDE {
    return active_wifi_ ? active_wifi_->connected() : false;
  }
  virtual const CellularNetwork* cellular_network() const OVERRIDE {
    return active_cellular_;
  }
  virtual bool cellular_connecting() const OVERRIDE {
    return active_cellular_ ? active_cellular_->connecting() : false;
  }
  virtual bool cellular_connected() const OVERRIDE {
    return active_cellular_ ? active_cellular_->connected() : false;
  }
  virtual const VirtualNetwork* virtual_network() const OVERRIDE {
    return active_virtual_;
  }
  virtual bool virtual_network_connecting() const OVERRIDE {
    return active_virtual_ ? active_virtual_->connecting() : false;
  }
  virtual bool virtual_network_connected() const OVERRIDE {
    return active_virtual_ ? active_virtual_->connected() : false;
  }
  virtual bool Connected() const OVERRIDE {
    return ethernet_connected() || wifi_connected() || cellular_connected();
  }
  virtual bool Connecting() const OVERRIDE {
    return ethernet_connecting() || wifi_connecting() || cellular_connecting();
  }
  virtual const WifiNetworkVector& wifi_networks() const OVERRIDE {
    return wifi_networks_;
  }
  virtual const WifiNetworkVector& remembered_wifi_networks() const OVERRIDE {
    return remembered_wifi_networks_;
  }
  virtual const CellularNetworkVector& cellular_networks() const OVERRIDE {
    return cellular_networks_;
  }
  virtual const VirtualNetworkVector& virtual_networks() const OVERRIDE {
    return virtual_networks_;
  }
  virtual const VirtualNetworkVector&
      remembered_virtual_networks() const OVERRIDE {
    return remembered_virtual_networks_;
  }
  virtual const Network* active_network() const OVERRIDE;
  virtual const Network* connected_network() const OVERRIDE;
  virtual const Network* connecting_network() const;
  virtual bool ethernet_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_CELLULAR);
  }
  virtual bool ethernet_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_CELLULAR);
  }
  virtual bool ethernet_busy() const OVERRIDE {
    return busy_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_busy() const OVERRIDE {
    return busy_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_busy() const OVERRIDE {
    return busy_devices_ & (1 << TYPE_CELLULAR);
  }
  virtual bool wifi_scanning() const OVERRIDE {
    return wifi_scanning_;
  }
  virtual bool offline_mode() const OVERRIDE { return offline_mode_; }
  virtual const std::string& IPAddress() const OVERRIDE;

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const OVERRIDE;
  NetworkDevice* FindNetworkDeviceByPath(const std::string& path);
  virtual const NetworkDevice* FindCellularDevice() const OVERRIDE;
  virtual const NetworkDevice* FindEthernetDevice() const OVERRIDE;
  virtual const NetworkDevice* FindWifiDevice() const OVERRIDE;
  virtual Network* FindNetworkByPath(const std::string& path) const OVERRIDE;
  virtual Network* FindNetworkByUniqueId(
      const std::string& unique_id) const OVERRIDE;
  WirelessNetwork* FindWirelessNetworkByPath(const std::string& path) const;
  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const OVERRIDE;
  Network* FindRememberedFromNetwork(const Network* network) const;
  virtual Network* FindRememberedNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual Network* FindRememberedNetworkByUniqueId(
      const std::string& unique_id) const OVERRIDE;

  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const OVERRIDE;
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const OVERRIDE;
  virtual void SignalCellularPlanPayment() OVERRIDE;
  virtual bool HasRecentCellularPlanPayment() OVERRIDE;
  virtual std::string GetCellularHomeCarrierId() const OVERRIDE;

  // virtual ChangePin implemented in derived classes.
  // virtual ChangeRequiredPin implemented in derived classes.
  // virtual EnterPin implemented in derived classes.
  // virtual UnblockPin implemented in derived classes.

  // virtual RequestCellularScan implemented in derived classes.
  // virtual RequestCellularRegister implemented in derived classes.
  // virtual SetCellularDataRoamingAllowed implemented in derived classes.
  // virtual IsCellularAlwaysInRoaming implemented in derived classes.
  // virtual RequestNetworkScan implemented in derived classes.
  // virtual GetWifiAccessPoints implemented in derived classes.

  virtual bool HasProfileType(NetworkProfileType type) const OVERRIDE;
  virtual void SetNetworkProfile(const std::string& service_path,
                                 NetworkProfileType type) OVERRIDE;
  virtual bool CanConnectToNetwork(const Network* network) const OVERRIDE;

  // Connect to an existing network.
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi) OVERRIDE;
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi, bool shared) OVERRIDE;
  virtual void ConnectToCellularNetwork(CellularNetwork* cellular) OVERRIDE;
  virtual void ConnectToVirtualNetwork(VirtualNetwork* vpn) OVERRIDE;

  // Request a network and connect to it.
  virtual void ConnectToUnconfiguredWifiNetwork(
      const std::string& ssid,
      ConnectionSecurity security,
      const std::string& passphrase,
      const EAPConfigData* eap_config,
      bool save_credentials,
      bool shared) OVERRIDE;

  virtual void ConnectToUnconfiguredVirtualNetwork(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type,
      const VPNConfigData& config) OVERRIDE;

  // virtual DisconnectFromNetwork implemented in derived classes.
  virtual void ForgetNetwork(const std::string& service_path) OVERRIDE;
  virtual void EnableEthernetNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableWifiNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableCellularNetworkDevice(bool enable) OVERRIDE;
  // virtual EnableOfflineMode implemented in derived classes.
  // virtual GetIPConfigs implemented in derived classes.
  // virtual SetIPConfig implemented in derived classes.
  virtual void SwitchToPreferredNetwork() OVERRIDE;
  virtual bool LoadOncNetworks(const std::string& onc_blob,
                               const std::string& passcode) OVERRIDE;
  virtual bool SetActiveNetwork(ConnectionType type,
                                const std::string& service_path) OVERRIDE;

 protected:
  typedef ObserverList<NetworkObserver> NetworkObserverList;
  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  typedef ObserverList<NetworkDeviceObserver> NetworkDeviceObserverList;
  typedef std::map<std::string, NetworkDeviceObserverList*>
      NetworkDeviceObserverMap;

  typedef std::map<std::string, Network*> NetworkMap;
  typedef std::map<std::string, int> PriorityMap;
  typedef std::map<std::string, NetworkDevice*> NetworkDeviceMap;
  typedef std::map<std::string, CellularDataPlanVector*> CellularDataPlanMap;

  struct NetworkProfile {
    NetworkProfile(const std::string& p, NetworkProfileType t)
        : path(p), type(t) {}
    std::string path;
    NetworkProfileType type;
    typedef std::set<std::string> ServiceList;
    ServiceList services;
  };
  typedef std::list<NetworkProfile> NetworkProfileList;

  struct ConnectData {
    ConnectData() :
        security(SECURITY_NONE),
        eap_method(EAP_METHOD_UNKNOWN),
        eap_auth(EAP_PHASE_2_AUTH_AUTO),
        eap_use_system_cas(false),
        save_credentials(false),
        profile_type(PROFILE_NONE) {}
    ConnectionSecurity security;
    std::string service_name;  // For example, SSID.
    std::string username;
    std::string passphrase;
    std::string otp;
    std::string group_name;
    std::string server_hostname;
    std::string server_ca_cert_nss_nickname;
    std::string client_cert_pkcs11_id;
    EAPMethod eap_method;
    EAPPhase2Auth eap_auth;
    bool eap_use_system_cas;
    std::string eap_identity;
    std::string eap_anonymous_identity;
    std::string psk_key;
    bool save_credentials;
    NetworkProfileType profile_type;
  };

  enum NetworkConnectStatus {
    CONNECT_SUCCESS,
    CONNECT_BAD_PASSPHRASE,
    CONNECT_FAILED
  };

  // Called from ConnectTo*Network.
  void NetworkConnectStartWifi(
      WifiNetwork* network, NetworkProfileType profile_type);
  void NetworkConnectStartVPN(VirtualNetwork* network);
  void NetworkConnectStart(Network* network, NetworkProfileType profile_type);
  // Called from CallConnectToNetwork.
  void NetworkConnectCompleted(Network* network,
                               NetworkConnectStatus status);
  // Called from CallRequestWifiNetworkAndConnect.
  void ConnectToWifiNetworkUsingConnectData(WifiNetwork* wifi);
  // Called from CallRequestVirtualNetworkAndConnect.
  void ConnectToVirtualNetworkUsingConnectData(VirtualNetwork* vpn);
  // Called from GetSignificantDataPlan.
  const CellularDataPlan* GetSignificantDataPlanFromVector(
      const CellularDataPlanVector* plans) const;
  CellularNetwork::DataLeft GetDataLeft(
      CellularDataPlanVector* data_plan_vector);
  // Takes ownership of |data_plan|.
  void UpdateCellularDataPlan(const std::string& service_path,
                              CellularDataPlanVector* data_plan_vector);

  // Network list management functions.
  void ClearActiveNetwork(ConnectionType type);
  void UpdateActiveNetwork(Network* network);
  void AddNetwork(Network* network);
  void DeleteNetwork(Network* network);
  void AddRememberedNetwork(Network* network);
  void DeleteRememberedNetwork(const std::string& service_path);
  void ClearNetworks();
  void ClearRememberedNetworks();
  void DeleteNetworks();
  void DeleteRememberedNetworks();
  void DeleteDevice(const std::string& device_path);
  void DeleteDeviceFromDeviceObserversMap(const std::string& device_path);

  // Profile management functions.
  void AddProfile(const std::string& profile_path,
                  NetworkProfileType profile_type);
  NetworkProfile* GetProfileForType(NetworkProfileType type);
  void SetProfileType(Network* network, NetworkProfileType type);
  void SetProfileTypeFromPath(Network* network);
  std::string GetProfilePath(NetworkProfileType type);

  // Notifications.
  void NotifyNetworkManagerChanged(bool force_update);
  void SignalNetworkManagerObservers();
  void NotifyNetworkChanged(const Network* network);
  void NotifyNetworkDeviceChanged(NetworkDevice* device, PropertyIndex index);
  void NotifyCellularDataPlanChanged();
  void NotifyPinOperationCompleted(PinOperationError error);
  void NotifyUserConnectionInitiated(const Network* network);

  // TPM related functions.
  void GetTpmInfo();
  const std::string& GetTpmSlot();
  const std::string& GetTpmPin();

  // Network manager observer list.
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list.
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // PIN operation observer list.
  ObserverList<PinOperationObserver> pin_operation_observers_;

  // User action observer list.
  ObserverList<UserActionObserver> user_action_observers_;

  // Network observer map.
  NetworkObserverMap network_observers_;

  // Network device observer map.
  NetworkDeviceObserverMap network_device_observers_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

  // List of profiles.
  NetworkProfileList profile_list_;

  // A service path based map of all visible Networks.
  NetworkMap network_map_;

  // A unique_id based map of all visible Networks.
  NetworkMap network_unique_id_map_;

  // A service path based map of all remembered Networks.
  NetworkMap remembered_network_map_;

  // A unique_id based map of all remembered Networks.
  NetworkMap remembered_network_unique_id_map_;

  // A list of services that we are awaiting updates for.
  PriorityMap network_update_requests_;

  // A device path based map of all NetworkDevices.
  NetworkDeviceMap device_map_;

  // A network service path based map of all CellularDataPlanVectors.
  CellularDataPlanMap data_plan_map_;

  // The ethernet network.
  EthernetNetwork* ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork* active_wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork* active_cellular_;

  // The list of available virtual networks.
  VirtualNetworkVector virtual_networks_;

  // The current connected (or connecting) virtual network.
  VirtualNetwork* active_virtual_;

  // The remembered virtual networks.
  VirtualNetworkVector remembered_virtual_networks_;

  // The path of the active profile (for retrieving remembered services).
  std::string active_profile_path_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current busy network devices. Bitwise flag of ConnectionTypes.
  // Busy means device is switching from enable/disable state.
  int busy_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  // True if we are currently scanning for wifi networks.
  bool wifi_scanning_;

  // Currently not implemented. TODO(stevenjb): implement or eliminate.
  bool offline_mode_;

  // True if access network library is locked.
  bool is_locked_;

  // TPM module user slot and PIN, needed by flimflam to access certificates.
  std::string tpm_slot_;
  std::string tpm_pin_;

  // Type of pending SIM operation, SIM_OPERATION_NONE otherwise.
  SimOperationType sim_operation_;

 private:
  // List of networks to move to the user profile once logged in.
  std::list<std::string> user_networks_;

  // Delayed task to notify a network change.
  CancelableTask* notify_task_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  // Temporary connection data for async connect calls.
  ConnectData connect_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplBase);
};

NetworkLibraryImplBase::NetworkLibraryImplBase()
    : ethernet_(NULL),
      active_wifi_(NULL),
      active_cellular_(NULL),
      active_virtual_(NULL),
      available_devices_(0),
      enabled_devices_(0),
      busy_devices_(0),
      connected_devices_(0),
      wifi_scanning_(false),
      offline_mode_(false),
      is_locked_(false),
      sim_operation_(SIM_OPERATION_NONE),
      notify_task_(NULL) {
  network_login_observer_.reset(new NetworkLoginObserver(this));
}

NetworkLibraryImplBase::~NetworkLibraryImplBase() {
  network_manager_observers_.Clear();
  data_plan_observers_.Clear();
  pin_operation_observers_.Clear();
  user_action_observers_.Clear();
  DeleteNetworks();
  DeleteRememberedNetworks();
  STLDeleteValues(&data_plan_map_);
  STLDeleteValues(&device_map_);
  STLDeleteValues(&network_device_observers_);
  STLDeleteValues(&network_observers_);
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

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

void NetworkLibraryImplBase::AddCellularDataPlanObserver(
    CellularDataPlanObserver* observer) {
  if (!data_plan_observers_.HasObserver(observer))
    data_plan_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveCellularDataPlanObserver(
    CellularDataPlanObserver* observer) {
  data_plan_observers_.RemoveObserver(observer);
}

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

// Use flimflam's ordering of the services to determine which type of
// network to return (i.e. don't assume priority of network types).
// Note: This does not include any virtual networks.
namespace {
Network* highest_priority(Network* a, Network*b) {
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
  Network* result = NULL;
  if (ethernet_ && ethernet_->is_active())
    result = ethernet_;
  if (active_wifi_ && active_wifi_->is_active())
    result = highest_priority(result, active_wifi_);
  if (active_cellular_ && active_cellular_->is_active())
    result = highest_priority(result, active_cellular_);
  if (active_virtual_ && active_virtual_->is_active())
    result = highest_priority(result, active_virtual_);
  return result;
}

const Network* NetworkLibraryImplBase::connected_network() const {
  Network* result = NULL;
  if (ethernet_ && ethernet_->connected())
    result = ethernet_;
  if (active_wifi_ && active_wifi_->connected())
    result = highest_priority(result, active_wifi_);
  if (active_cellular_ && active_cellular_->connected())
    result = highest_priority(result, active_cellular_);
  return result;
}

// Connecting order in logical prefernce.
const Network* NetworkLibraryImplBase::connecting_network() const {
  if (ethernet_connecting())
    return ethernet_network();
  else if (wifi_connecting())
    return wifi_network();
  else if (cellular_connecting())
    return cellular_network();
  return NULL;
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
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second && iter->second->type() == TYPE_CELLULAR)
      return iter->second;
  }
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindEthernetDevice() const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second->type() == TYPE_ETHERNET)
      return iter->second;
  }
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindWifiDevice() const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second->type() == TYPE_WIFI)
      return iter->second;
  }
  return NULL;
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
      (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR))
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

Network* NetworkLibraryImplBase::FindRememberedNetworkByUniqueId(
    const std::string& unique_id) const {
  NetworkMap::const_iterator found =
      remembered_network_unique_id_map_.find(unique_id);
  if (found != remembered_network_unique_id_map_.end())
    return found->second;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////
// Data Plans.

const CellularDataPlanVector* NetworkLibraryImplBase::GetDataPlans(
    const std::string& path) const {
  CellularDataPlanMap::const_iterator iter = data_plan_map_.find(path);
  if (iter == data_plan_map_.end())
    return NULL;
  // If we need a new plan, then ignore any data plans we have.
  CellularNetwork* cellular = FindCellularNetworkByPath(path);
  if (cellular && cellular->needs_new_plan())
    return NULL;
  return iter->second;
}

const CellularDataPlan* NetworkLibraryImplBase::GetSignificantDataPlan(
    const std::string& path) const {
  const CellularDataPlanVector* plans = GetDataPlans(path);
  if (plans)
    return GetSignificantDataPlanFromVector(plans);
  return NULL;
}

const CellularDataPlan* NetworkLibraryImplBase::
GetSignificantDataPlanFromVector(const CellularDataPlanVector* plans) const {
  const CellularDataPlan* significant = NULL;
  for (CellularDataPlanVector::const_iterator iter = plans->begin();
       iter != plans->end(); ++iter) {
    // Set significant to the first plan or to first non metered base plan.
    if (significant == NULL ||
        significant->plan_type == CELLULAR_DATA_PLAN_METERED_BASE)
      significant = *iter;
  }
  return significant;
}

CellularNetwork::DataLeft NetworkLibraryImplBase::GetDataLeft(
    CellularDataPlanVector* data_plan_vector) {
  const CellularDataPlan* plan =
      GetSignificantDataPlanFromVector(data_plan_vector);
  if (!plan)
    return CellularNetwork::DATA_UNKNOWN;
  if (plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED) {
    base::TimeDelta remaining = plan->remaining_time();
    if (remaining <= base::TimeDelta::FromSeconds(0))
      return CellularNetwork::DATA_NONE;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataVeryLowSecs))
      return CellularNetwork::DATA_VERY_LOW;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataLowSecs))
      return CellularNetwork::DATA_LOW;
    return CellularNetwork::DATA_NORMAL;
  } else if (plan->plan_type == CELLULAR_DATA_PLAN_METERED_PAID ||
             plan->plan_type == CELLULAR_DATA_PLAN_METERED_BASE) {
    int64 remaining = plan->remaining_data();
    if (remaining <= 0)
      return CellularNetwork::DATA_NONE;
    if (remaining <= kCellularDataVeryLowBytes)
      return CellularNetwork::DATA_VERY_LOW;
    // For base plans, we do not care about low data.
    if (remaining <= kCellularDataLowBytes &&
        plan->plan_type != CELLULAR_DATA_PLAN_METERED_BASE)
      return CellularNetwork::DATA_LOW;
    return CellularNetwork::DATA_NORMAL;
  }
  return CellularNetwork::DATA_UNKNOWN;
}

void NetworkLibraryImplBase::UpdateCellularDataPlan(
    const std::string& service_path,
    CellularDataPlanVector* data_plan_vector) {
  VLOG(1) << "Updating cellular data plans for: " << service_path;
  // Find and delete any existing data plans associated with |service_path|.
  CellularDataPlanMap::iterator found = data_plan_map_.find(service_path);
  if (found != data_plan_map_.end())
    delete found->second;  // This will delete existing data plans.
  // Takes ownership of |data_plan_vector|.
  data_plan_map_[service_path] = data_plan_vector;

  // Now, update any matching cellular network's cached data
  CellularNetwork* cellular = FindCellularNetworkByPath(service_path);
  if (cellular) {
    CellularNetwork::DataLeft data_left;
    // If the network needs a new plan, then there's no data.
    if (cellular->needs_new_plan())
      data_left = CellularNetwork::DATA_NONE;
    else
      data_left = GetDataLeft(data_plan_vector);
    VLOG(2) << " Data left: " << data_left
            << " Need plan: " << cellular->needs_new_plan();
    cellular->set_data_left(data_left);
  }
  NotifyCellularDataPlanChanged();
}

void NetworkLibraryImplBase::SignalCellularPlanPayment() {
  DCHECK(!HasRecentCellularPlanPayment());
  cellular_plan_payment_time_ = base::Time::Now();
}

bool NetworkLibraryImplBase::HasRecentCellularPlanPayment() {
  return (base::Time::Now() -
          cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
}

std::string NetworkLibraryImplBase::GetCellularHomeCarrierId() const {
  const NetworkDevice* cellular = FindCellularDevice();
  if (cellular)
    return cellular->home_provider_id();
  return std::string();
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

// Note: currently there is no way to change the profile of a network
// unless it is visible (i.e. in network_map_). We change the profile by
// setting the Profile property of the visible network.
// See chromium-os:15523.
void NetworkLibraryImplBase::SetNetworkProfile(
    const std::string& service_path, NetworkProfileType type) {
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    LOG(WARNING) << "SetNetworkProfile: Network not found: " << service_path;
    return;
  }
  if (network->profile_type() == type) {
    LOG(WARNING) << "Remembered Network: " << service_path
                 << " already in profile: " << type;
    return;
  }
  if (network->RequiresUserProfile() && type == PROFILE_SHARED) {
    LOG(WARNING) << "SetNetworkProfile to Shared for non sharable network: "
                 << service_path;
    return;
  }
  VLOG(1) << "Setting network: " << network->name()
          << " to profile type: " << type;
  SetProfileType(network, type);
  NotifyNetworkManagerChanged(false);
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
  // flimflam uses the PIN if it needs to access certificates in the TPM and
  // ignores it otherwise.
  if (wifi->encryption() == SECURITY_8021X) {
    // If the TPM initialization has not completed, GetTpmPin() will return
    // an empty value, in which case we do not want to clear the PIN since
    // that will cause flimflam to flag the network as unconfigured.
    // TODO(stevenjb): We may want to delay attempting to connect, or fail
    // immediately, rather than let the network layer attempt a connection.
    std::string tpm_pin = GetTpmPin();
    if (!tpm_pin.empty())
      wifi->SetCertificatePin(tpm_pin);
  }
  NetworkConnectStart(wifi, profile_type);
}

void NetworkLibraryImplBase::NetworkConnectStartVPN(VirtualNetwork* vpn) {
  // flimflam needs the TPM PIN for some VPN networks to access client
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
  network->set_connecting(true);
  // Distinguish between user-initiated connection attempts
  // and auto-connect.
  network->set_connection_started(true);
  NotifyNetworkManagerChanged(true);  // Forced update.
  VLOG(1) << "Requesting connect to network: " << network->service_path()
          << " profile type: " << profile_type;
  // Specify the correct profile for wifi networks (if specified or unset).
  if (network->type() == TYPE_WIFI &&
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
      network->set_error(ERROR_BAD_PASSPHRASE);
    } else {
      network->set_error(ERROR_CONNECT_FAILED);
    }
    NotifyNetworkManagerChanged(true);  // Forced update.
    return;
  }

  VLOG(1) << "Connected to service: " << network->name();

  // If the user asked not to save credentials, flimflam will have
  // forgotten them.  Wipe our cache as well.
  if (!network->save_credentials())
    network->EraseCredentials();

  ClearActiveNetwork(network->type());
  UpdateActiveNetwork(network);

  // Notify observers.
  NotifyNetworkManagerChanged(true);  // Forced update.
  NotifyUserConnectionInitiated(network);
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

bool NetworkLibraryImplBase::LoadOncNetworks(const std::string& onc_blob,
                                             const std::string& passcode) {
  // TODO(gspencer): Add support for decrypting onc files. crbug.com/19397
  OncNetworkParser parser(onc_blob);

  for (int i = 0; i < parser.GetCertificatesSize(); i++) {
    // Insert each of the available certs into the certificate DB.
    if (!parser.ParseCertificate(i)) {
      DLOG(WARNING) << "Cannot parse certificate in ONC file";
      return false;
    }
  }

  for (int i = 0; i < parser.GetNetworkConfigsSize(); i++) {
    // Parse Open Network Configuration blob into a temporary Network object.
    scoped_ptr<Network> network(parser.ParseNetwork(i));
    if (!network.get()) {
      DLOG(WARNING) << "Cannot parse networks in ONC file";
      return false;
    }

    DictionaryValue dict;
    for (Network::PropertyMap::const_iterator props =
             network->property_map_.begin();
         props != network->property_map_.end(); ++props) {
      std::string key =
          NativeNetworkParser::property_mapper()->GetKey(props->first);
      if (!key.empty())
        dict.SetWithoutPathExpansion(key, props->second->DeepCopy());
    }

    CallConfigureService(network->unique_id(), &dict);
  }
  return parser.GetNetworkConfigsSize() != 0;
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

// Note: sometimes flimflam still returns networks when the device type is
// disabled. Always check the appropriate enabled() state before adding
// networks to a list or setting an active network so that we do not show them
// in the UI.

// This relies on services being requested from flimflam in priority order,
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
  } else if (type == TYPE_VPN) {
    virtual_networks_.push_back(static_cast<VirtualNetwork*>(network));
  }
  // Do not set the active network here. Wait until we parse the network.
}

// Deletes a network. It must already be removed from any lists.
void NetworkLibraryImplBase::DeleteNetwork(Network* network) {
  CHECK(network_map_.find(network->service_path()) == network_map_.end());
  if (network->type() == TYPE_CELLULAR) {
    // Find and delete any existing data plans associated with |service_path|.
    CellularDataPlanMap::iterator found =
        data_plan_map_.find(network->service_path());
    if (found != data_plan_map_.end()) {
      CellularDataPlanVector* data_plan_vector = found->second;
      delete data_plan_vector;
      data_plan_map_.erase(found);
    }
  }
  delete network;
}

void NetworkLibraryImplBase::AddRememberedNetwork(Network* network) {
  std::pair<NetworkMap::iterator, bool> result =
      remembered_network_map_.insert(
          std::make_pair(network->service_path(), network));
  DCHECK(result.second);  // Should only get called with new network.
  if (network->type() == TYPE_WIFI) {
    remembered_wifi_networks_.push_back(
        static_cast<WifiNetwork*>(network));
  } else if (network->type() == TYPE_VPN) {
    remembered_virtual_networks_.push_back(
        static_cast<VirtualNetwork*>(network));
  } else {
    NOTREACHED();
  }
  // Find service path in profiles. Flimflam does not set the Profile
  // property for remembered networks, only active networks.
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.services.find(network->service_path()) !=
        profile.services.end()) {
      network->set_profile_path(profile.path);
      network->set_profile_type(profile.type);
      VLOG(1) << "AddRememberedNetwork: " << network->service_path()
              << " profile: " << profile.path;
      break;
    }
  }
  DCHECK(!network->profile_path().empty())
      << "Service path not in any profile: " << network->service_path();
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

  // Update any associated network service before removing from profile
  // so that flimflam doesn't recreate the service (e.g. when we disconenct it).
  Network* network = FindNetworkByUniqueId(remembered_network->unique_id());
  if (network) {
    // Clear the stored credentials for any forgotten networks.
    network->EraseCredentials();
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

  delete remembered_network;
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::ClearNetworks() {
  network_map_.clear();
  network_unique_id_map_.clear();
  ethernet_ = NULL;
  active_wifi_ = NULL;
  active_cellular_ = NULL;
  active_virtual_ = NULL;
  wifi_networks_.clear();
  cellular_networks_.clear();
  virtual_networks_.clear();
}

void NetworkLibraryImplBase::ClearRememberedNetworks() {
  remembered_network_map_.clear();
  remembered_network_unique_id_map_.clear();
  remembered_wifi_networks_.clear();
  remembered_virtual_networks_.clear();
}

void NetworkLibraryImplBase::DeleteNetworks() {
  STLDeleteValues(&network_map_);
  ClearNetworks();
}

void NetworkLibraryImplBase::DeleteRememberedNetworks() {
  STLDeleteValues(&remembered_network_map_);
  ClearRememberedNetworks();
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

void NetworkLibraryImplBase::AddProfile(
    const std::string& profile_path, NetworkProfileType profile_type) {
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

// We call this any time something in NetworkLibrary changes.
// TODO(stevenjb): We should consider breaking this into multiple
// notifications, e.g. connection state, devices, services, etc.
void NetworkLibraryImplBase::NotifyNetworkManagerChanged(bool force_update) {
  // Cancel any pending signals.
  if (notify_task_) {
    notify_task_->Cancel();
    notify_task_ = NULL;
  }
  if (force_update) {
    // Signal observers now.
    SignalNetworkManagerObservers();
  } else {
    // Schedule a delayed signal to limit the frequency of notifications.
    notify_task_ = NewRunnableMethod(
        this, &NetworkLibraryImplBase::SignalNetworkManagerObservers);
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, notify_task_,
                                   kNetworkNotifyDelayMs);
  }
}

void NetworkLibraryImplBase::SignalNetworkManagerObservers() {
  notify_task_ = NULL;
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

void NetworkLibraryImplBase::NotifyCellularDataPlanChanged() {
  FOR_EACH_OBSERVER(CellularDataPlanObserver,
                    data_plan_observers_,
                    OnCellularDataPlanChanged(this));
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

////////////////////////////////////////////////////////////////////////////

class NetworkLibraryImplCros : public NetworkLibraryImplBase  {
 public:
  NetworkLibraryImplCros();
  virtual ~NetworkLibraryImplCros();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE { return true; }

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE;

  virtual void CallConfigureService(const std::string& identifier,
                                    const DictionaryValue* info) OVERRIDE;
  virtual void CallConnectToNetwork(Network* network) OVERRIDE;
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) OVERRIDE;
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type) OVERRIDE;
  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path,
      const std::string& service_path) OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibrary implementation.

  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) OVERRIDE;
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) OVERRIDE;
  virtual void EnterPin(const std::string& pin) OVERRIDE;
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) OVERRIDE;
  virtual void RequestCellularScan() OVERRIDE;
  virtual void RequestCellularRegister(const std::string& network_id) OVERRIDE;
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE;
  virtual bool IsCellularAlwaysInRoaming() OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE;
  virtual void CallRemoveNetwork(const Network* network) OVERRIDE;

  virtual void EnableOfflineMode(bool enable) OVERRIDE;

  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // Calbacks.
  static void NetworkStatusChangedHandler(
      void* object, const char* path, const char* key, const GValue* value);
  void UpdateNetworkStatus(
      const std::string& path, const std::string& key, const Value& value);

  static void NetworkDevicePropertyChangedHandler(
      void* object, const char* path, const char* key, const GValue* gvalue);
  void UpdateNetworkDeviceStatus(
      const std::string& path, const std::string& key, const Value& value);

  static void PinOperationCallback(void* object,
                                   const char* path,
                                   NetworkMethodErrorType error,
                                   const char* error_message);

  static void CellularRegisterCallback(void* object,
                                       const char* path,
                                       NetworkMethodErrorType error,
                                       const char* error_message);

  static void ConfigureServiceCallback(void* object,
                                       const char* service_path,
                                       NetworkMethodErrorType error,
                                       const char* error_message);

  static void NetworkConnectCallback(void* object,
                                     const char* service_path,
                                     NetworkMethodErrorType error,
                                     const char* error_message);

  static void WifiServiceUpdateAndConnect(
      void* object, const char* service_path, GHashTable* ghash);
  static void VPNServiceUpdateAndConnect(
      void* object, const char* service_path, GHashTable* ghash);

  static void NetworkManagerStatusChangedHandler(
      void* object, const char* path, const char* key, const GValue* value);
  static void NetworkManagerUpdate(
      void* object, const char* manager_path, GHashTable* ghash);

  static void DataPlanUpdateHandler(
      void* object,
      const char* modem_service_path,
      const chromeos::CellularDataPlanList* data_plan_list);

  static void NetworkServiceUpdate(
      void* object, const char* service_path, GHashTable* ghash);
  static void RememberedNetworkServiceUpdate(
      void* object, const char* service_path, GHashTable* ghash);
  static void ProfileUpdate(
      void* object, const char* profile_path, GHashTable* ghash);
  static void NetworkDeviceUpdate(
      void* object, const char* device_path, GHashTable* ghash);

 private:
  // This processes all Manager update messages.
  void NetworkManagerStatusChanged(const char* key, const Value* value);
  void ParseNetworkManager(const DictionaryValue& dict);
  void UpdateTechnologies(const ListValue* technologies, int* bitfieldp);
  void UpdateAvailableTechnologies(const ListValue* technologies);
  void UpdateEnabledTechnologies(const ListValue* technologies);
  void UpdateConnectedTechnologies(const ListValue* technologies);

  // Update network lists.
  void UpdateNetworkServiceList(const ListValue* services);
  void UpdateWatchedNetworkServiceList(const ListValue* services);
  Network* ParseNetwork(const std::string& service_path,
                        const DictionaryValue& info);

  void UpdateRememberedNetworks(const ListValue* profiles);
  void RequestRememberedNetworksUpdate();
  void UpdateRememberedServiceList(const char* profile_path,
                                   const ListValue* profile_entries);
  Network* ParseRememberedNetwork(const std::string& service_path,
                                  const DictionaryValue& info);

  // NetworkDevice list management functions.
  void UpdateNetworkDeviceList(const ListValue* devices);
  void ParseNetworkDevice(const std::string& device_path,
                          const DictionaryValue& info);

  // Empty device observer to ensure that device property updates are received.
  class NetworkLibraryDeviceObserver : public NetworkDeviceObserver {
   public:
    virtual ~NetworkLibraryDeviceObserver() {}
    virtual void OnNetworkDeviceChanged(
        NetworkLibrary* cros, const NetworkDevice* device) OVERRIDE {}
  };

  typedef std::map<std::string, chromeos::NetworkPropertiesMonitor>
          NetworkPropertiesMonitorMap;

  // For monitoring network manager status changes.
  NetworkPropertiesMonitor network_manager_monitor_;

  // For monitoring data plan changes to the connected cellular network.
  DataPlanUpdateMonitor data_plan_monitor_;

  // Network device observer.
  scoped_ptr<NetworkLibraryDeviceObserver> network_device_observer_;

  // Map of monitored networks.
  NetworkPropertiesMonitorMap montitored_networks_;

  // Map of monitored devices.
  NetworkPropertiesMonitorMap montitored_devices_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplCros);
};

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplCros::NetworkLibraryImplCros()
    : NetworkLibraryImplBase(),
      network_manager_monitor_(NULL),
      data_plan_monitor_(NULL) {
}

NetworkLibraryImplCros::~NetworkLibraryImplCros() {
  if (network_manager_monitor_)
    chromeos::DisconnectNetworkPropertiesMonitor(network_manager_monitor_);
  if (data_plan_monitor_)
    chromeos::DisconnectDataPlanUpdateMonitor(data_plan_monitor_);
  for (NetworkPropertiesMonitorMap::iterator iter =
           montitored_networks_.begin();
       iter != montitored_networks_.end(); ++iter) {
    chromeos::DisconnectNetworkPropertiesMonitor(iter->second);
  }
  for (NetworkPropertiesMonitorMap::iterator iter =
           montitored_devices_.begin();
       iter != montitored_devices_.end(); ++iter) {
    chromeos::DisconnectNetworkPropertiesMonitor(iter->second);
  }
}

void NetworkLibraryImplCros::Init() {
  CHECK(CrosLibrary::Get()->libcros_loaded())
      << "libcros must be loaded before NetworkLibraryImplCros::Init()";
  // First, get the currently available networks. This data is cached
  // on the connman side, so the call should be quick.
  VLOG(1) << "Requesting initial network manager info from libcros.";
  chromeos::RequestNetworkManagerProperties(&NetworkManagerUpdate, this);
  network_manager_monitor_ =
      chromeos::MonitorNetworkManagerProperties(
          &NetworkManagerStatusChangedHandler, this);
  data_plan_monitor_ =
      chromeos::MonitorCellularDataPlan(&DataPlanUpdateHandler, this);
  // Always have at least one device obsever so that device updates are
  // always received.
  network_device_observer_.reset(new NetworkLibraryDeviceObserver());
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase implementation.

void NetworkLibraryImplCros::MonitorNetworkStart(
    const std::string& service_path) {
  if (montitored_networks_.find(service_path) == montitored_networks_.end()) {
    chromeos::NetworkPropertiesMonitor monitor =
        chromeos::MonitorNetworkServiceProperties(
            &NetworkStatusChangedHandler, service_path.c_str(), this);
    montitored_networks_[service_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkStop(
    const std::string& service_path) {
  NetworkPropertiesMonitorMap::iterator iter =
      montitored_networks_.find(service_path);
  if (iter != montitored_networks_.end()) {
    chromeos::DisconnectNetworkPropertiesMonitor(iter->second);
    montitored_networks_.erase(iter);
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStart(
    const std::string& device_path) {
  if (montitored_devices_.find(device_path) == montitored_devices_.end()) {
    chromeos::NetworkPropertiesMonitor monitor =
        chromeos::MonitorNetworkDeviceProperties(
            &NetworkDevicePropertyChangedHandler, device_path.c_str(), this);
    montitored_devices_[device_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStop(
    const std::string& device_path) {
  NetworkPropertiesMonitorMap::iterator iter =
      montitored_devices_.find(device_path);
  if (iter != montitored_devices_.end()) {
    chromeos::DisconnectNetworkPropertiesMonitor(iter->second);
    montitored_devices_.erase(iter);
  }
}

// static callback
void NetworkLibraryImplCros::NetworkStatusChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (key == NULL || gvalue == NULL || path == NULL || object == NULL)
    return;
  scoped_ptr<Value> value(ConvertGlibValue(gvalue));
  networklib->UpdateNetworkStatus(std::string(path), std::string(key), *value);
}

void NetworkLibraryImplCros::UpdateNetworkStatus(
    const std::string& path, const std::string& key, const Value& value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Network* network = FindNetworkByPath(path);
  if (network) {
    VLOG(2) << "UpdateNetworkStatus: " << network->name() << "." << key;
    bool prev_connected = network->connected();
    if (!network->UpdateStatus(key, value, NULL)) {
      LOG(WARNING) << "UpdateNetworkStatus: Error updating: "
                   << path << "." << key;
    }
    // If we just connected, this may have been added to remembered list.
    if (!prev_connected && network->connected())
      RequestRememberedNetworksUpdate();
    NotifyNetworkChanged(network);
    // Anything observing the manager needs to know about any service change.
    NotifyNetworkManagerChanged(false);  // Not forced.
  }
}

// static callback
void NetworkLibraryImplCros::NetworkDevicePropertyChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (key == NULL || gvalue == NULL || path == NULL || object == NULL)
    return;
  scoped_ptr<Value> value(ConvertGlibValue(gvalue));
  networklib->UpdateNetworkDeviceStatus(std::string(path),
                                        std::string(key),
                                        *value);
}

void NetworkLibraryImplCros::UpdateNetworkDeviceStatus(
    const std::string& path, const std::string& key, const Value& value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  NetworkDevice* device = FindNetworkDeviceByPath(path);
  if (device) {
    VLOG(2) << "UpdateNetworkDeviceStatus: " << device->name() << "." << key;
    PropertyIndex index = PROPERTY_INDEX_UNKNOWN;
    if (device->UpdateStatus(key, value, &index)) {
      if (index == PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING) {
        if (!device->data_roaming_allowed() && IsCellularAlwaysInRoaming()) {
          SetCellularDataRoamingAllowed(true);
        } else {
          bool settings_value;
          if (CrosSettings::Get()->GetBoolean(
                  kSignedDataRoamingEnabled, &settings_value) &&
              device->data_roaming_allowed() != settings_value) {
            // Switch back to signed settings value.
            SetCellularDataRoamingAllowed(settings_value);
            return;
          }
        }
      }
    } else {
      VLOG(1) << "UpdateNetworkDeviceStatus: Failed to update: "
              << path << "." << key;
    }
    // Notify only observers on device property change.
    NotifyNetworkDeviceChanged(device, index);
    // If a device's power state changes, new properties may become defined.
    if (index == PROPERTY_INDEX_POWERED)
      chromeos::RequestNetworkDeviceProperties(path.c_str(),
                                               &NetworkDeviceUpdate,
                                               this);
  }
}

/////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase connect implementation.

// static callback
void NetworkLibraryImplCros::ConfigureServiceCallback(
    void* object,
    const char* service_path,
    NetworkMethodErrorType error,
    const char* error_message) {
  if (error != NETWORK_METHOD_ERROR_NONE) {
    LOG(WARNING) << "Error from ConfigureService callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
  }
}

void NetworkLibraryImplCros::CallConfigureService(const std::string& identifier,
                                                  const DictionaryValue* info) {
  GHashTable* ghash = ConvertDictionaryValueToGValueMap(info);
  chromeos::ConfigureService(identifier.c_str(), ghash,
                             ConfigureServiceCallback, this);
}

// static callback
void NetworkLibraryImplCros::NetworkConnectCallback(
    void* object,
    const char* service_path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkConnectStatus status;
  if (error == NETWORK_METHOD_ERROR_NONE) {
    status = CONNECT_SUCCESS;
  } else {
    LOG(WARNING) << "Error from ServiceConnect callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
    if (error_message &&
        strcmp(error_message, flimflam::kErrorPassphraseRequiredMsg) == 0) {
      status = CONNECT_BAD_PASSPHRASE;
    } else {
      status = CONNECT_FAILED;
    }
  }
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  Network* network = networklib->FindNetworkByPath(std::string(service_path));
  if (!network) {
    LOG(ERROR) << "No network for path: " << service_path;
    return;
  }
  networklib->NetworkConnectCompleted(network, status);
}

void NetworkLibraryImplCros::CallConnectToNetwork(Network* network) {
  DCHECK(network);
  chromeos::RequestNetworkServiceConnect(network->service_path().c_str(),
                                         NetworkConnectCallback, this);
}

// static callback
void NetworkLibraryImplCros::WifiServiceUpdateAndConnect(
    void* object, const char* service_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && ghash) {
    scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
    Network* network =
        networklib->ParseNetwork(std::string(service_path), *(dict.get()));
    DCHECK_EQ(network->type(), TYPE_WIFI);
    networklib->ConnectToWifiNetworkUsingConnectData(
        static_cast<WifiNetwork*>(network));
  }
}

void NetworkLibraryImplCros::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  // Asynchronously request service properties and call
  // WifiServiceUpdateAndConnect.
  chromeos::RequestHiddenWifiNetworkProperties(
      ssid.c_str(),
      SecurityToString(security),
      WifiServiceUpdateAndConnect,
      this);
}

// static callback
void NetworkLibraryImplCros::VPNServiceUpdateAndConnect(
    void* object, const char* service_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && ghash) {
    VLOG(1) << "Connecting to new VPN Service: " << service_path;
    scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
    Network* network =
        networklib->ParseNetwork(std::string(service_path), *(dict.get()));
    DCHECK_EQ(network->type(), TYPE_VPN);
    networklib->ConnectToVirtualNetworkUsingConnectData(
        static_cast<VirtualNetwork*>(network));
  } else {
    LOG(WARNING) << "Unable to create VPN Service: " << service_path;
  }
}

void NetworkLibraryImplCros::CallRequestVirtualNetworkAndConnect(
    const std::string& service_name,
    const std::string& server_hostname,
    ProviderType provider_type) {
  chromeos::RequestVirtualNetworkProperties(
      service_name.c_str(),
      server_hostname.c_str(),
      ProviderTypeToString(provider_type),
      VPNServiceUpdateAndConnect,
      this);
}

void NetworkLibraryImplCros::CallDeleteRememberedNetwork(
    const std::string& profile_path,
    const std::string& service_path) {
  chromeos::DeleteServiceFromProfile(
      profile_path.c_str(), service_path.c_str());
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplCros::ChangePin(const std::string& old_pin,
                                       const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_PIN;
  chromeos::RequestChangePin(cellular->device_path().c_str(),
                             old_pin.c_str(), new_pin.c_str(),
                             PinOperationCallback, this);
}

void NetworkLibraryImplCros::ChangeRequirePin(bool require_pin,
                                              const std::string& pin) {
  VLOG(1) << "ChangeRequirePin require_pin: " << require_pin
          << " pin: " << pin;
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangeRequirePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_REQUIRE_PIN;
  chromeos::RequestRequirePin(cellular->device_path().c_str(),
                              pin.c_str(), require_pin,
                              PinOperationCallback, this);
}

void NetworkLibraryImplCros::EnterPin(const std::string& pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling EnterPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  chromeos::RequestEnterPin(cellular->device_path().c_str(),
                            pin.c_str(),
                            PinOperationCallback, this);
}

void NetworkLibraryImplCros::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling UnblockPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  chromeos::RequestUnblockPin(cellular->device_path().c_str(),
                              puk.c_str(), new_pin.c_str(),
                              PinOperationCallback, this);
}

// static callback
void NetworkLibraryImplCros::PinOperationCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  PinOperationError pin_error;
  VLOG(1) << "PinOperationCallback, error: " << error
          << " error_msg: " << error_message;
  if (error == chromeos::NETWORK_METHOD_ERROR_NONE) {
    pin_error = PIN_ERROR_NONE;
    VLOG(1) << "Pin operation completed successfuly";
  } else {
    if (error_message &&
        (strcmp(error_message, flimflam::kErrorIncorrectPinMsg) == 0 ||
         strcmp(error_message, flimflam::kErrorPinRequiredMsg) == 0)) {
      pin_error = PIN_ERROR_INCORRECT_CODE;
    } else if (error_message &&
               strcmp(error_message, flimflam::kErrorPinBlockedMsg) == 0) {
      pin_error = PIN_ERROR_BLOCKED;
    } else {
      pin_error = PIN_ERROR_UNKNOWN;
      NOTREACHED() << "Unknown PIN error: " << error_message;
    }
  }
  networklib->NotifyPinOperationCompleted(pin_error);
}

void NetworkLibraryImplCros::RequestCellularScan() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequestCellularScan method w/o cellular device.";
    return;
  }
  chromeos::ProposeScan(cellular->device_path().c_str());
}

void NetworkLibraryImplCros::RequestCellularRegister(
    const std::string& network_id) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling CellularRegister method w/o cellular device.";
    return;
  }
  chromeos::RequestCellularRegister(cellular->device_path().c_str(),
                                    network_id.c_str(),
                                    CellularRegisterCallback,
                                    this);
}

// static callback
void NetworkLibraryImplCros::CellularRegisterCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  // TODO(dpolukhin): Notify observers about network registration status
  // but not UI doesn't assume such notification so just ignore result.
}

void NetworkLibraryImplCros::SetCellularDataRoamingAllowed(bool new_value) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling SetCellularDataRoamingAllowed method "
        "w/o cellular device.";
    return;
  }
  scoped_ptr<GValue> gvalue(ConvertBoolToGValue(new_value));
  chromeos::SetNetworkDevicePropertyGValue(
      cellular->device_path().c_str(),
      flimflam::kCellularAllowRoamingProperty, gvalue.get());
}

bool NetworkLibraryImplCros::IsCellularAlwaysInRoaming() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling IsCellularAlwaysInRoaming method "
        "w/o cellular device.";
    return false;
  }
  const std::string& home_provider_name = cellular->home_provider_name();
  for (size_t i = 0; i < arraysize(kAlwaysInRoamingOperators); i++) {
    if (home_provider_name == kAlwaysInRoamingOperators[i])
      return true;
  }
  return false;
}

void NetworkLibraryImplCros::RequestNetworkScan() {
  if (wifi_enabled()) {
    wifi_scanning_ = true;  // Cleared when updates are received.
    chromeos::RequestNetworkScan(flimflam::kTypeWifi);
  }
  if (cellular_network())
    cellular_network()->RefreshDataPlansIfNeeded();
  // Make sure all Manager info is up to date. This will also update
  // remembered networks and visible services.
  chromeos::RequestNetworkManagerProperties(&NetworkManagerUpdate, this);
}

bool NetworkLibraryImplCros::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceNetworkList* network_list = chromeos::GetDeviceNetworkList();
  if (network_list == NULL)
    return false;
  result->clear();
  result->reserve(network_list->network_size);
  const base::Time now = base::Time::Now();
  for (size_t i = 0; i < network_list->network_size; ++i) {
    DCHECK(network_list->networks[i].address);
    DCHECK(network_list->networks[i].name);
    WifiAccessPoint ap;
    ap.mac_address = SafeString(network_list->networks[i].address);
    ap.name = SafeString(network_list->networks[i].name);
    ap.timestamp = now -
        base::TimeDelta::FromSeconds(network_list->networks[i].age_seconds);
    ap.signal_strength = network_list->networks[i].strength;
    ap.channel = network_list->networks[i].channel;
    result->push_back(ap);
  }
  chromeos::FreeDeviceNetworkList(network_list);
  return true;
}

void NetworkLibraryImplCros::DisconnectFromNetwork(const Network* network) {
  DCHECK(network);
  // Asynchronous disconnect request. Network state will be updated through
  // the network manager once disconnect completes.
  chromeos::RequestNetworkServiceDisconnect(network->service_path().c_str());
}

void NetworkLibraryImplCros::CallEnableNetworkDeviceType(
    ConnectionType device, bool enable) {
  busy_devices_ |= 1 << device;
  chromeos::RequestNetworkDeviceEnable(
      ConnectionTypeToString(device), enable);
}

void NetworkLibraryImplCros::CallRemoveNetwork(const Network* network) {
  const char* service_path = network->service_path().c_str();
  if (network->connected())
    chromeos::RequestNetworkServiceDisconnect(service_path);
  chromeos::RequestRemoveNetworkService(service_path);
}

void NetworkLibraryImplCros::EnableOfflineMode(bool enable) {
  // If network device is already enabled/disabled, then don't do anything.
  if (chromeos::SetOfflineMode(enable))
    offline_mode_ = enable;
}

NetworkIPConfigVector NetworkLibraryImplCros::GetIPConfigs(
    const std::string& device_path,
    std::string* hardware_address,
    HardwareAddressFormat format) {
  DCHECK(hardware_address);
  hardware_address->clear();
  NetworkIPConfigVector ipconfig_vector;
  if (!device_path.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; ++i) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        ipconfig_vector.push_back(
            NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                            ipconfig.netmask, ipconfig.gateway,
                            ipconfig.name_servers));
      }
      *hardware_address = ipconfig_status->hardware_address;
      FreeIPConfigStatus(ipconfig_status);
    }
  }

  for (size_t i = 0; i < hardware_address->size(); ++i)
    (*hardware_address)[i] = toupper((*hardware_address)[i]);
  if (format == FORMAT_COLON_SEPARATED_HEX) {
    if (hardware_address->size() % 2 == 0) {
      std::string output;
      for (size_t i = 0; i < hardware_address->size(); ++i) {
        if ((i != 0) && (i % 2 == 0))
          output.push_back(':');
        output.push_back((*hardware_address)[i]);
      }
      *hardware_address = output;
    }
  } else {
    DCHECK_EQ(format, FORMAT_RAW_HEX);
  }
  return ipconfig_vector;
}

void NetworkLibraryImplCros::SetIPConfig(const NetworkIPConfig& ipconfig) {
  if (ipconfig.device_path.empty())
    return;

  IPConfig* ipconfig_dhcp = NULL;
  IPConfig* ipconfig_static = NULL;

  IPConfigStatus* ipconfig_status =
      chromeos::ListIPConfigs(ipconfig.device_path.c_str());
  if (ipconfig_status) {
    for (int i = 0; i < ipconfig_status->size; ++i) {
      if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_DHCP)
        ipconfig_dhcp = &ipconfig_status->ips[i];
      else if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
        ipconfig_static = &ipconfig_status->ips[i];
    }
  }

  IPConfigStatus* ipconfig_status2 = NULL;
  if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP) {
    // If switching from static to dhcp, create new dhcp ip config.
    if (!ipconfig_dhcp)
      chromeos::AddIPConfig(ipconfig.device_path.c_str(),
                            chromeos::IPCONFIG_TYPE_DHCP);
    // User wants DHCP now. So delete the static ip config.
    if (ipconfig_static)
      chromeos::RemoveIPConfig(ipconfig_static);
  } else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4) {
    // If switching from dhcp to static, create new static ip config.
    if (!ipconfig_static) {
      chromeos::AddIPConfig(ipconfig.device_path.c_str(),
                            chromeos::IPCONFIG_TYPE_IPV4);
      // Now find the newly created IP config.
      ipconfig_status2 =
          chromeos::ListIPConfigs(ipconfig.device_path.c_str());
      if (ipconfig_status2) {
        for (int i = 0; i < ipconfig_status2->size; ++i) {
          if (ipconfig_status2->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
            ipconfig_static = &ipconfig_status2->ips[i];
        }
      }
    }
    if (ipconfig_static) {
      // Save any changed details.
      if (ipconfig.address != ipconfig_static->address) {
        scoped_ptr<GValue> gvalue(ConvertStringToGValue(ipconfig.address));
        chromeos::SetNetworkIPConfigPropertyGValue(
            ipconfig_static->path, flimflam::kAddressProperty, gvalue.get());
      }
      if (ipconfig.netmask != ipconfig_static->netmask) {
        int prefixlen = ipconfig.GetPrefixLength();
        if (prefixlen == -1) {
          VLOG(1) << "IP config prefixlen is invalid for netmask "
                  << ipconfig.netmask;
        } else {
          scoped_ptr<GValue> gvalue(ConvertIntToGValue(prefixlen));
          chromeos::SetNetworkIPConfigPropertyGValue(
              ipconfig_static->path,
              flimflam::kPrefixlenProperty, gvalue.get());
        }
      }
      if (ipconfig.gateway != ipconfig_static->gateway) {
        scoped_ptr<GValue> gvalue(ConvertStringToGValue(ipconfig.gateway));
        chromeos::SetNetworkIPConfigPropertyGValue(
            ipconfig_static->path, flimflam::kGatewayProperty, gvalue.get());
      }
      if (ipconfig.name_servers != ipconfig_static->name_servers) {
        scoped_ptr<GValue> gvalue(ConvertStringToGValue(ipconfig.name_servers));
        chromeos::SetNetworkIPConfigPropertyGValue(
            ipconfig_static->path,
            flimflam::kNameServersProperty, gvalue.get());
      }
      // Remove dhcp ip config if there is one.
      if (ipconfig_dhcp)
        chromeos::RemoveIPConfig(ipconfig_dhcp);
    }
  }

  if (ipconfig_status)
    chromeos::FreeIPConfigStatus(ipconfig_status);
  if (ipconfig_status2)
    chromeos::FreeIPConfigStatus(ipconfig_status2);
}

/////////////////////////////////////////////////////////////////////////////
// Network Manager functions.

// static
void NetworkLibraryImplCros::NetworkManagerStatusChangedHandler(
    void* object, const char* path, const char* key, const GValue* gvalue) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  scoped_ptr<Value> value(ConvertGlibValue(gvalue));
  networklib->NetworkManagerStatusChanged(key, value.get());
}

// This processes all Manager update messages.
void NetworkLibraryImplCros::NetworkManagerStatusChanged(
    const char* key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeTicks start = base::TimeTicks::Now();
  if (!key)
    return;
  VLOG(1) << "NetworkManagerStatusChanged: KEY=" << key;
  int index = NativeNetworkParser::property_mapper()->Get(key);
  switch (index) {
    case PROPERTY_INDEX_STATE:
      // Currently we ignore the network manager state.
      break;
    case PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateAvailableTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_ENABLED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateEnabledTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_CONNECTED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateConnectedTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_DEFAULT_TECHNOLOGY:
      // Currently we ignore DefaultTechnology.
      break;
    case PROPERTY_INDEX_OFFLINE_MODE: {
      DCHECK_EQ(value->GetType(), Value::TYPE_BOOLEAN);
      value->GetAsBoolean(&offline_mode_);
      NotifyNetworkManagerChanged(false);  // Not forced.
      break;
    }
    case PROPERTY_INDEX_ACTIVE_PROFILE: {
      std::string prev = active_profile_path_;
      DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
      value->GetAsString(&active_profile_path_);
      VLOG(1) << "Active Profile: " << active_profile_path_;
      if (active_profile_path_ != prev &&
          active_profile_path_ != kSharedProfilePath)
        SwitchToPreferredNetwork();
      break;
    }
    case PROPERTY_INDEX_PROFILES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateRememberedNetworks(vlist);
      RequestRememberedNetworksUpdate();
      break;
    }
    case PROPERTY_INDEX_SERVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_SERVICE_WATCH_LIST: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateWatchedNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_DEVICE:
    case PROPERTY_INDEX_DEVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkDeviceList(vlist);
      break;
    }
    case PROPERTY_INDEX_PORTAL_URL:
    case PROPERTY_INDEX_CHECK_PORTAL_LIST:
    case PROPERTY_INDEX_ARP_GATEWAY:
      // Currently we ignore PortalURL, CheckPortalList and ArpGateway.
      break;
    default:
      LOG(WARNING) << "Manager: Unhandled key: " << key;
      break;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - start;
  VLOG(2) << "NetworkManagerStatusChanged: time: "
          << delta.InMilliseconds() << " ms.";
  HISTOGRAM_TIMES("CROS_NETWORK_UPDATE", delta);
}

// static
void NetworkLibraryImplCros::NetworkManagerUpdate(
    void* object, const char* manager_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!ghash) {
    LOG(ERROR) << "Error retrieving manager properties: " << manager_path;
    return;
  }
  VLOG(1) << "Received NetworkManagerUpdate.";
  scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
  networklib->ParseNetworkManager(*(dict.get()));
}

void NetworkLibraryImplCros::ParseNetworkManager(const DictionaryValue& dict) {
  for (DictionaryValue::key_iterator iter = dict.begin_keys();
       iter != dict.end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = dict.GetWithoutPathExpansion(key, &value);
    CHECK(res);
    NetworkManagerStatusChanged(key.c_str(), value);
  }
  // If there is no Profiles entry, request remembered networks here.
  if (!dict.HasKey(flimflam::kProfilesProperty))
    RequestRememberedNetworksUpdate();
}

// static
void NetworkLibraryImplCros::DataPlanUpdateHandler(
    void* object,
    const char* modem_service_path,
    const chromeos::CellularDataPlanList* data_plan_list) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (modem_service_path && data_plan_list) {
    // Copy contents of |data_plan_list| from libcros to |data_plan_vector|.
    CellularDataPlanVector* data_plan_vector = new CellularDataPlanVector;
    for (size_t i = 0; i < data_plan_list->plans_size; ++i) {
      const CellularDataPlanInfo* info(data_plan_list->GetCellularDataPlan(i));
      CellularDataPlan* plan = new CellularDataPlan(*info);
      data_plan_vector->push_back(plan);
      VLOG(2) << " Plan: " << plan->GetPlanDesciption()
              << " : " << plan->GetDataRemainingDesciption();
    }
    // |data_plan_vector| will become owned by networklib.
    networklib->UpdateCellularDataPlan(std::string(modem_service_path),
                                       data_plan_vector);
  }
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateTechnologies(
    const ListValue* technologies, int* bitfieldp) {
  DCHECK(bitfieldp);
  if (!technologies)
    return;
  int bitfield = 0;
  for (ListValue::const_iterator iter = technologies->begin();
       iter != technologies->end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    if (!technology.empty()) {
      ConnectionType type =
          NativeNetworkParser::ParseConnectionType(technology);
      bitfield |= 1 << type;
    }
  }
  *bitfieldp = bitfield;
  NotifyNetworkManagerChanged(false);  // Not forced.
}

void NetworkLibraryImplCros::UpdateAvailableTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &available_devices_);
}

void NetworkLibraryImplCros::UpdateEnabledTechnologies(
    const ListValue* technologies) {
  int old_enabled_devices = enabled_devices_;
  UpdateTechnologies(technologies, &enabled_devices_);
  busy_devices_ &= ~(old_enabled_devices ^ enabled_devices_);
  if (!ethernet_enabled())
    ethernet_ = NULL;
  if (!wifi_enabled()) {
    active_wifi_ = NULL;
    wifi_networks_.clear();
  }
  if (!cellular_enabled()) {
    active_cellular_ = NULL;
    cellular_networks_.clear();
  }
}

void NetworkLibraryImplCros::UpdateConnectedTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &connected_devices_);
}

////////////////////////////////////////////////////////////////////////////

// Update all network lists, and request associated service updates.
void NetworkLibraryImplCros::UpdateNetworkServiceList(
    const ListValue* services) {
  // TODO(stevenjb): Wait for wifi_scanning_ to be false.
  // Copy the list of existing networks to "old" and clear the map and lists.
  NetworkMap old_network_map = network_map_;
  ClearNetworks();
  // Clear the list of update requests.
  int network_priority_order = 0;
  network_update_requests_.clear();
  // wifi_scanning_ will remain false unless we request a network update.
  wifi_scanning_ = false;
  // |services| represents a complete list of visible networks.
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      // If we find the network in "old", add it immediately to the map
      // and lists. Otherwise it will get added when NetworkServiceUpdate
      // calls ParseNetwork.
      NetworkMap::iterator found = old_network_map.find(service_path);
      if (found != old_network_map.end()) {
        AddNetwork(found->second);
        old_network_map.erase(found);
      }
      // Always request network updates.
      // TODO(stevenjb): Investigate why we are missing updates then
      // rely on watched network updates and only request updates here for
      // new networks.
      // Use update_request map to store network priority.
      network_update_requests_[service_path] = network_priority_order++;
      wifi_scanning_ = true;
      chromeos::RequestNetworkServiceProperties(service_path.c_str(),
                                                &NetworkServiceUpdate,
                                                this);
    }
  }
  // Iterate through list of remaining networks that are no longer in the
  // list and delete them or update their status and re-add them to the list.
  for (NetworkMap::iterator iter = old_network_map.begin();
       iter != old_network_map.end(); ++iter) {
    Network* network = iter->second;
    VLOG(2) << "Delete Network: " << network->name()
            << " State = " << network->GetStateString()
            << " connecting = " << network->connecting()
            << " connection_started = " << network->connection_started();
    if (network->failed() && network->notify_failure()) {
      // We have not notified observers of a connection failure yet.
      AddNetwork(network);
    } else if (network->connecting() && network->connection_started()) {
      // Network was in connecting state; set state to failed.
      VLOG(2) << "Removed network was connecting: " << network->name();
      network->SetState(STATE_FAILURE);
      AddNetwork(network);
    } else {
      VLOG(2) << "Deleting removed network: " << network->name()
              << " State = " << network->GetStateString();
      DeleteNetwork(network);
    }
  }
  // If the last network has disappeared, nothing else will
  // have notified observers, so do it now.
  if (services->empty())
    NotifyNetworkManagerChanged(true);  // Forced update
}

// Request updates for watched networks. Does not affect network lists.
// Existing networks will be updated. There should not be any new networks
// in this list, but if there are they will be added appropriately.
void NetworkLibraryImplCros::UpdateWatchedNetworkServiceList(
    const ListValue* services) {
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      VLOG(1) << "Watched Service: " << service_path;
      chromeos::RequestNetworkServiceProperties(service_path.c_str(),
                                                &NetworkServiceUpdate,
                                                this);
    }
  }
}

// static
void NetworkLibraryImplCros::NetworkServiceUpdate(
    void* object, const char* service_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!ghash)
      return;  // Network no longer in visible list, ignore.
    scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
    networklib->ParseNetwork(std::string(service_path), *(dict.get()));
  }
}

// Called from NetworkServiceUpdate and WifiServiceUpdateAndConnect.
Network* NetworkLibraryImplCros::ParseNetwork(
    const std::string& service_path, const DictionaryValue& info) {
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    NativeNetworkParser parser;
    network = parser.CreateNetworkFromInfo(service_path, info);
    AddNetwork(network);
  } else {
    // Erase entry from network_unique_id_map_ in case unique id changes.
    if (!network->unique_id().empty())
      network_unique_id_map_.erase(network->unique_id());
    if (network->network_parser())
      network->network_parser()->UpdateNetworkFromInfo(info, network);
  }

  if (!network->unique_id().empty())
    network_unique_id_map_[network->unique_id()] = network;

  SetProfileTypeFromPath(network);

  UpdateActiveNetwork(network);

  // Copy remembered credentials if required.
  Network* remembered = FindRememberedFromNetwork(network);
  if (remembered)
    network->CopyCredentialsFromRemembered(remembered);

  // Find and erase entry in update_requests, and set network priority.
  PriorityMap::iterator found2 = network_update_requests_.find(service_path);
  if (found2 != network_update_requests_.end()) {
    network->priority_order_ = found2->second;
    network_update_requests_.erase(found2);
    if (network_update_requests_.empty()) {
      // Clear wifi_scanning_ when we have no pending requests.
      wifi_scanning_ = false;
    }
  } else {
    // TODO(stevenjb): Enable warning once UpdateNetworkServiceList is fixed.
    // LOG(WARNING) << "ParseNetwork called with no update request entry: "
    //              << service_path;
  }

  VLOG(1) << "ParseNetwork: " << network->name()
          << " path: " << network->service_path()
          << " profile: " << network->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.
  return network;
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateRememberedNetworks(
    const ListValue* profiles) {
  VLOG(1) << "UpdateRememberedNetworks";
  // Update the list of profiles.
  profile_list_.clear();
  for (ListValue::const_iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    std::string profile_path;
    (*iter)->GetAsString(&profile_path);
    if (profile_path.empty()) {
      LOG(WARNING) << "Bad or empty profile path.";
      continue;
    }
    NetworkProfileType profile_type;
    if (profile_path == kSharedProfilePath)
      profile_type = PROFILE_SHARED;
    else
      profile_type = PROFILE_USER;
    AddProfile(profile_path, profile_type);
  }
}

void NetworkLibraryImplCros::RequestRememberedNetworksUpdate() {
  VLOG(1) << "RequestRememberedNetworksUpdate";
  // Delete all remembered networks. We delete them because
  // RequestNetworkProfileProperties is asynchronous and may invoke
  // UpdateRememberedServiceList multiple times (once per profile).
  // We can do this safely because we do not save any local state for
  // remembered networks. This list updates infrequently.
  DeleteRememberedNetworks();
  // Request remembered networks from each profile. Later entries will
  // override earlier entries, so default/local entries will override
  // user entries (as desired).
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    VLOG(1) << " Requesting Profile: " << profile.path;
    chromeos::RequestNetworkProfileProperties(
        profile.path.c_str(), &ProfileUpdate, this);
  }
}

// static
void NetworkLibraryImplCros::ProfileUpdate(
    void* object, const char* profile_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!ghash) {
    LOG(ERROR) << "Error retrieving profile: " << profile_path;
    return;
  }
  VLOG(1) << "Received ProfileUpdate for: " << profile_path;
  scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
  ListValue* entries(NULL);
  dict->GetList(flimflam::kEntriesProperty, &entries);
  DCHECK(entries);
  networklib->UpdateRememberedServiceList(profile_path, entries);
}

void NetworkLibraryImplCros::UpdateRememberedServiceList(
    const char* profile_path, const ListValue* profile_entries) {
  DCHECK(profile_path);
  VLOG(1) << "UpdateRememberedServiceList for path: " << profile_path;
  NetworkProfileList::iterator iter1;
  for (iter1 = profile_list_.begin(); iter1 != profile_list_.end(); ++iter1) {
    if (iter1->path == profile_path)
      break;
  }
  if (iter1 == profile_list_.end()) {
    // This can happen if flimflam gets restarted while Chrome is running.
    LOG(WARNING) << "Profile not in list: " << profile_path;
    return;
  }
  NetworkProfile& profile = *iter1;
  // |profile_entries| is a list of remembered networks from |profile_path|.
  profile.services.clear();
  for (ListValue::const_iterator iter2 = profile_entries->begin();
       iter2 != profile_entries->end(); ++iter2) {
    std::string service_path;
    (*iter2)->GetAsString(&service_path);
    if (service_path.empty()) {
      LOG(WARNING) << "Empty service path in profile.";
      continue;
    }
    VLOG(1) << " Remembered service: " << service_path;
    // Add service to profile list.
    profile.services.insert(service_path);
    // Request update for remembered network.
    chromeos::RequestNetworkProfileEntryProperties(
        profile_path,
        service_path.c_str(),
        &RememberedNetworkServiceUpdate,
        this);
  }
}

// static
void NetworkLibraryImplCros::RememberedNetworkServiceUpdate(
    void* object, const char* service_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!ghash) {
      // Remembered network no longer exists.
      networklib->DeleteRememberedNetwork(std::string(service_path));
    } else {
      scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
      networklib->ParseRememberedNetwork(
          std::string(service_path), *(dict.get()));
    }
  }
}

// Returns NULL if |service_path| refers to a network that is not a
// remembered type. Called from RememberedNetworkServiceUpdate.
Network* NetworkLibraryImplCros::ParseRememberedNetwork(
    const std::string& service_path, const DictionaryValue& info) {
  Network* remembered;
  NetworkMap::iterator found = remembered_network_map_.find(service_path);
  if (found != remembered_network_map_.end()) {
    remembered = found->second;
    // Erase entry from network_unique_id_map_ in case unique id changes.
    if (!remembered->unique_id().empty())
      remembered_network_unique_id_map_.erase(remembered->unique_id());
    if (remembered->network_parser())
      remembered->network_parser()->UpdateNetworkFromInfo(info, remembered);
  } else {
    NativeNetworkParser parser;
    remembered = parser.CreateNetworkFromInfo(service_path, info);
    if (remembered->type() == TYPE_WIFI || remembered->type() == TYPE_VPN) {
      AddRememberedNetwork(remembered);
    } else {
      LOG(WARNING) << "Ignoring remembered network: " << service_path
                   << " Type: " << ConnectionTypeToString(remembered->type());
      delete remembered;
      return NULL;
    }
  }

  if (!remembered->unique_id().empty())
    remembered_network_unique_id_map_[remembered->unique_id()] = remembered;

  SetProfileTypeFromPath(remembered);

  VLOG(1) << "ParseRememberedNetwork: " << remembered->name()
          << " path: " << remembered->service_path()
          << " profile: " << remembered->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.

  if (remembered->type() == TYPE_VPN) {
    // VPNs are only stored in profiles. If we don't have a network for it,
    // request one.
    if (!FindNetworkByUniqueId(remembered->unique_id())) {
      VirtualNetwork* vpn = static_cast<VirtualNetwork*>(remembered);
      std::string provider_type = ProviderTypeToString(vpn->provider_type());
      VLOG(1) << "Requesting VPN: " << vpn->name()
              << " Server: " << vpn->server_hostname()
              << " Type: " << provider_type;
      chromeos::RequestVirtualNetworkProperties(
          vpn->name().c_str(),
          vpn->server_hostname().c_str(),
          provider_type.c_str(),
          NetworkServiceUpdate,
          this);
    }
  }

  return remembered;
}

////////////////////////////////////////////////////////////////////////////
// NetworkDevice list management functions.

// Update device list, and request associated device updates.
// |devices| represents a complete list of devices.
void NetworkLibraryImplCros::UpdateNetworkDeviceList(const ListValue* devices) {
  NetworkDeviceMap old_device_map = device_map_;
  device_map_.clear();
  VLOG(2) << "Updating Device List.";
  for (ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    (*iter)->GetAsString(&device_path);
    if (!device_path.empty()) {
      NetworkDeviceMap::iterator found = old_device_map.find(device_path);
      if (found != old_device_map.end()) {
        VLOG(2) << " Adding existing device: " << device_path;
        CHECK(found->second) << "Attempted to add NULL device pointer";
        device_map_[device_path] = found->second;
        old_device_map.erase(found);
      }
      chromeos::RequestNetworkDeviceProperties(device_path.c_str(),
                                               &NetworkDeviceUpdate,
                                               this);
    }
  }
  // Delete any old devices that no longer exist.
  for (NetworkDeviceMap::iterator iter = old_device_map.begin();
       iter != old_device_map.end(); ++iter) {
    DeleteDeviceFromDeviceObserversMap(iter->first);
    // Delete device.
    delete iter->second;
  }
}

// static
void NetworkLibraryImplCros::NetworkDeviceUpdate(
    void* object, const char* device_path, GHashTable* ghash) {
  DCHECK(CrosLibrary::Get()->libcros_loaded());
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (device_path) {
    if (!ghash) {
      // device no longer exists.
      networklib->DeleteDevice(std::string(device_path));
    } else {
      scoped_ptr<DictionaryValue> dict(ConvertGHashTable(ghash));
      networklib->ParseNetworkDevice(std::string(device_path), *(dict.get()));
    }
  }
}

void NetworkLibraryImplCros::ParseNetworkDevice(const std::string& device_path,
                                                const DictionaryValue& info) {
  NetworkDeviceMap::iterator found = device_map_.find(device_path);
  NetworkDevice* device;
  if (found != device_map_.end()) {
    device = found->second;
    device->device_parser()->UpdateDeviceFromInfo(info, device);
  } else {
    NativeNetworkDeviceParser parser;
    device = parser.CreateDeviceFromInfo(device_path, info);
    VLOG(2) << " Adding device: " << device_path;
    if (device) {
      device_map_[device_path] = device;
    }
    CHECK(device) << "Attempted to add NULL device for path: " << device_path;
  }
  VLOG(1) << "ParseNetworkDevice:" << device->name();
  if (device && device->type() == TYPE_CELLULAR) {
    if (!device->data_roaming_allowed() && IsCellularAlwaysInRoaming()) {
      SetCellularDataRoamingAllowed(true);
    } else {
      bool settings_value;
      if (CrosSettings::Get()->GetBoolean(
              kSignedDataRoamingEnabled, &settings_value) &&
          device->data_roaming_allowed() != settings_value) {
        // Switch back to signed settings value.
        SetCellularDataRoamingAllowed(settings_value);
      }
    }
  }
  NotifyNetworkManagerChanged(false);  // Not forced.
  AddNetworkDeviceObserver(device_path, network_device_observer_.get());
}

////////////////////////////////////////////////////////////////////////////

class NetworkLibraryImplStub : public NetworkLibraryImplBase {
 public:
  NetworkLibraryImplStub();
  virtual ~NetworkLibraryImplStub();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE { return false; }

  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE {}
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE {}
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE {}
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE {}

  virtual void CallConfigureService(const std::string& identifier,
                                    const DictionaryValue* info) OVERRIDE {}
  virtual void CallConnectToNetwork(Network* network) OVERRIDE;
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) OVERRIDE;
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      ProviderType provider_type) OVERRIDE;

  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path,
      const std::string& service_path) OVERRIDE {}

  // NetworkLibrary implementation.

  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) OVERRIDE;
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) OVERRIDE;
  virtual void EnterPin(const std::string& pin) OVERRIDE;
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) OVERRIDE;

  virtual void RequestCellularScan() OVERRIDE {}
  virtual void RequestCellularRegister(
      const std::string& network_id) OVERRIDE {}
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE {}
  virtual bool IsCellularAlwaysInRoaming() OVERRIDE { return false; }
  virtual void RequestNetworkScan() OVERRIDE;

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;

  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE {}

  virtual void CallRemoveNetwork(const Network* network) OVERRIDE {}

  virtual void EnableOfflineMode(bool enable) OVERRIDE {
    offline_mode_ = enable;
  }

  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) OVERRIDE;

 private:
  void AddStubNetwork(Network* network, NetworkProfileType profile_type);
  void AddStubRememberedNetwork(Network* network);
  void ConnectToNetwork(Network* network);

  std::string ip_address_;
  std::string hardware_address_;
  NetworkIPConfigVector ip_configs_;
  std::string pin_;
  bool pin_required_;
  bool pin_entered_;
  int64 connect_delay_ms_;
  int network_priority_order_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplStub);
};

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplStub::NetworkLibraryImplStub()
    : ip_address_("1.1.1.1"),
      hardware_address_("01:23:45:67:89:ab"),
      pin_(""),
      pin_required_(false),
      pin_entered_(false),
      connect_delay_ms_(0),
      network_priority_order_(0) {
}

NetworkLibraryImplStub::~NetworkLibraryImplStub() {
}

void NetworkLibraryImplStub::Init() {
  is_locked_ = false;

  // Devices
  int devices =
      (1 << TYPE_ETHERNET) | (1 << TYPE_WIFI) | (1 << TYPE_CELLULAR);
  available_devices_ = devices;
  enabled_devices_ = devices;
  connected_devices_ = devices;

  NetworkDevice* cellular = new NetworkDevice("cellular");
  cellular->type_ = TYPE_CELLULAR;
  cellular->imsi_ = "123456789012345";
  device_map_["cellular"] = cellular;

  CellularApn apn;
  apn.apn = "apn";
  apn.network_id = "network_id";
  apn.username = "username";
  apn.password = "password";
  apn.name = "name";
  apn.localized_name = "localized_name";
  apn.language = "language";

  CellularApnList apn_list;
  apn_list.push_back(apn);

  NetworkDevice* cellular_gsm = new NetworkDevice("cellular_gsm");
  cellular_gsm->type_ = TYPE_CELLULAR;
  cellular_gsm->set_technology_family(TECHNOLOGY_FAMILY_GSM);
  cellular_gsm->imsi_ = "123456789012345";
  cellular_gsm->set_sim_pin_required(SIM_PIN_REQUIRED);
  cellular_gsm->set_provider_apn_list(apn_list);
  device_map_["cellular_gsm"] = cellular_gsm;

  // Profiles
  AddProfile("default", PROFILE_SHARED);
  AddProfile("user", PROFILE_USER);

  // Networks
  // If these change, the expectations in network_library_unittest and
  // network_menu_icon_unittest need to be changed also.

  // Networks are added in priority order.
  network_priority_order_ = 0;

  Network* ethernet = new EthernetNetwork("eth1");
  ethernet->set_name("Fake Ethernet");
  ethernet->set_is_active(true);
  ethernet->set_connected(true);
  AddStubNetwork(ethernet, PROFILE_SHARED);

  WifiNetwork* wifi1 = new WifiNetwork("wifi1");
  wifi1->set_name("Fake WiFi1");
  wifi1->set_strength(100);
  wifi1->set_connected(true);
  wifi1->set_encryption(SECURITY_NONE);
  AddStubNetwork(wifi1, PROFILE_SHARED);

  WifiNetwork* wifi2 = new WifiNetwork("wifi2");
  wifi2->set_name("Fake WiFi2");
  wifi2->set_strength(70);
  wifi2->set_encryption(SECURITY_NONE);
  AddStubNetwork(wifi2, PROFILE_SHARED);

  WifiNetwork* wifi3 = new WifiNetwork("wifi3");
  wifi3->set_name("Fake WiFi3 Encrypted with a long name");
  wifi3->set_strength(60);
  wifi3->set_encryption(SECURITY_WEP);
  wifi3->set_passphrase_required(true);
  AddStubNetwork(wifi3, PROFILE_USER);

  WifiNetwork* wifi4 = new WifiNetwork("wifi4");
  wifi4->set_name("Fake WiFi4 802.1x");
  wifi4->set_strength(50);
  wifi4->set_connectable(false);
  wifi4->set_encryption(SECURITY_8021X);
  wifi4->SetEAPMethod(EAP_METHOD_PEAP);
  wifi4->SetEAPIdentity("nobody@google.com");
  wifi4->SetEAPPassphrase("password");
  AddStubNetwork(wifi4, PROFILE_NONE);

  WifiNetwork* wifi5 = new WifiNetwork("wifi5");
  wifi5->set_name("Fake WiFi5 UTF-8 SSID ");
  wifi5->SetSsid("Fake WiFi5 UTF-8 SSID \u3042\u3044\u3046");
  wifi5->set_strength(25);
  AddStubNetwork(wifi5, PROFILE_NONE);

  WifiNetwork* wifi6 = new WifiNetwork("wifi6");
  wifi6->set_name("Fake WiFi6 latin-1 SSID ");
  wifi6->SetSsid("Fake WiFi6 latin-1 SSID \xc0\xcb\xcc\xd6\xfb");
  wifi6->set_strength(20);
  AddStubNetwork(wifi6, PROFILE_NONE);

  WifiNetwork* wifi7 = new WifiNetwork("wifi7");
  wifi7->set_name("Fake Wifi7 (policy-managed)");
  wifi7->set_strength(100);
  wifi7->set_connectable(false);
  wifi7->set_passphrase_required(true);
  wifi7->set_encryption(SECURITY_8021X);
  wifi7->SetEAPMethod(EAP_METHOD_PEAP);
  wifi7->SetEAPIdentity("enterprise@example.com");
  wifi7->SetEAPPassphrase("password");
  NetworkUIData wifi7_ui_data;
  wifi7_ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_DEVICE_POLICY);
  wifi7_ui_data.FillDictionary(wifi7->ui_data());
  AddStubNetwork(wifi7, PROFILE_USER);

  CellularNetwork* cellular1 = new CellularNetwork("cellular1");
  cellular1->set_name("Fake Cellular 1");
  cellular1->set_strength(100);
  cellular1->set_connected(true);
  cellular1->set_activation_state(ACTIVATION_STATE_ACTIVATED);
  cellular1->set_payment_url(std::string("http://www.google.com"));
  cellular1->set_usage_url(std::string("http://www.google.com"));
  cellular1->set_network_technology(NETWORK_TECHNOLOGY_EVDO);
  AddStubNetwork(cellular1, PROFILE_NONE);

  CellularNetwork* cellular2 = new CellularNetwork("cellular2");
  cellular2->set_name("Fake Cellular 2");
  cellular2->set_strength(50);
  cellular2->set_activation_state(ACTIVATION_STATE_NOT_ACTIVATED);
  cellular2->set_network_technology(NETWORK_TECHNOLOGY_UMTS);
  cellular2->set_roaming_state(ROAMING_STATE_ROAMING);
  AddStubNetwork(cellular2, PROFILE_NONE);

  CellularNetwork* cellular3 = new CellularNetwork("cellular3");
  cellular3->set_name("Fake Cellular 3 (policy-managed)");
  cellular3->set_device_path(cellular->device_path());
  cellular3->set_activation_state(ACTIVATION_STATE_ACTIVATED);
  cellular3->set_network_technology(NETWORK_TECHNOLOGY_EVDO);
  NetworkUIData cellular3_ui_data;
  cellular3_ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_USER_POLICY);
  cellular3_ui_data.FillDictionary(cellular3->ui_data());
  AddStubNetwork(cellular3, PROFILE_NONE);

  CellularNetwork* cellular4 = new CellularNetwork("cellular4");
  cellular4->set_name("Fake Cellular 4 (policy-managed)");
  cellular4->set_device_path(cellular_gsm->device_path());
  cellular4->set_activation_state(ACTIVATION_STATE_ACTIVATED);
  cellular4->set_network_technology(NETWORK_TECHNOLOGY_GSM);
  NetworkUIData cellular4_ui_data;
  cellular4_ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_USER_POLICY);
  cellular4_ui_data.FillDictionary(cellular4->ui_data());
  AddStubNetwork(cellular4, PROFILE_NONE);

  CellularDataPlan* base_plan = new CellularDataPlan();
  base_plan->plan_name = "Base plan";
  base_plan->plan_type = CELLULAR_DATA_PLAN_METERED_BASE;
  base_plan->plan_data_bytes = 100ll * 1024 * 1024;
  base_plan->data_bytes_used = base_plan->plan_data_bytes / 4;

  CellularDataPlan* paid_plan = new CellularDataPlan();
  paid_plan->plan_name = "Paid plan";
  paid_plan->plan_type = CELLULAR_DATA_PLAN_METERED_PAID;
  paid_plan->plan_data_bytes = 5ll * 1024 * 1024 * 1024;
  paid_plan->data_bytes_used = paid_plan->plan_data_bytes / 2;

  CellularDataPlanVector* data_plan_vector = new CellularDataPlanVector;
  data_plan_vector->push_back(base_plan);
  data_plan_vector->push_back(paid_plan);
  UpdateCellularDataPlan(cellular1->service_path(), data_plan_vector);

  VirtualNetwork* vpn1 = new VirtualNetwork("vpn1");
  vpn1->set_name("Fake VPN1");
  vpn1->set_server_hostname("vpn1server.fake.com");
  vpn1->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_PSK);
  vpn1->set_username("VPN User 1");
  AddStubNetwork(vpn1, PROFILE_USER);

  VirtualNetwork* vpn2 = new VirtualNetwork("vpn2");
  vpn2->set_name("Fake VPN2");
  vpn2->set_server_hostname("vpn2server.fake.com");
  vpn2->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  vpn2->set_username("VPN User 2");
  AddStubNetwork(vpn2, PROFILE_USER);

  VirtualNetwork* vpn3 = new VirtualNetwork("vpn3");
  vpn3->set_name("Fake VPN3");
  vpn3->set_server_hostname("vpn3server.fake.com");
  vpn3->set_provider_type(PROVIDER_TYPE_OPEN_VPN);
  AddStubNetwork(vpn3, PROFILE_USER);

  VirtualNetwork* vpn4 = new VirtualNetwork("vpn4");
  vpn4->set_name("Fake VPN4 (policy-managed)");
  vpn4->set_server_hostname("vpn4server.fake.com");
  vpn4->set_provider_type(PROVIDER_TYPE_OPEN_VPN);
  NetworkUIData vpn4_ui_data;
  vpn4_ui_data.set_onc_source(NetworkUIData::ONC_SOURCE_DEVICE_POLICY);
  vpn4_ui_data.FillDictionary(vpn4->ui_data());
  AddStubNetwork(vpn4, PROFILE_USER);

  wifi_scanning_ = false;
  offline_mode_ = false;

  // Ensure our active network is connected and vice versa, otherwise our
  // autotest browser_tests sometimes conclude the device is offline.
  CHECK(active_network()->connected());
  CHECK(connected_network()->is_active());

  std::string test_blob(
        "{"
        "  \"NetworkConfigurations\": ["
        "    {"
        "      \"GUID\": \"guid\","
        "      \"Type\": \"WiFi\","
        "      \"WiFi\": {"
        "        \"Security\": \"WEP\","
        "        \"SSID\": \"MySSID\","
        "      }"
        "    }"
        "  ],"
        "  \"Certificates\": []"
        "}");
  LoadOncNetworks(test_blob, "");
}

////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplStub private methods.

void NetworkLibraryImplStub::AddStubNetwork(
    Network* network, NetworkProfileType profile_type) {
  network->priority_order_ = network_priority_order_++;
  network->CalculateUniqueId();
  if (!network->unique_id().empty())
    network_unique_id_map_[network->unique_id()] = network;
  AddNetwork(network);
  UpdateActiveNetwork(network);
  SetProfileType(network, profile_type);
  AddStubRememberedNetwork(network);
}

// Add a remembered network to the appropriate profile if specified.
void NetworkLibraryImplStub::AddStubRememberedNetwork(Network* network) {
  if (network->profile_type() == PROFILE_NONE)
    return;

  Network* remembered = FindRememberedFromNetwork(network);
  if (remembered) {
    // This network is already in the rememebred list. Check to see if the
    // type has changed.
    if (remembered->profile_type() == network->profile_type())
      return;  // Same type, nothing to do.
    // Delete the existing remembered network from the previous profile.
    DeleteRememberedNetwork(remembered->service_path());
    remembered = NULL;
  }

  NetworkProfile* profile = GetProfileForType(network->profile_type());
  if (profile) {
    profile->services.insert(network->service_path());
  } else {
    LOG(ERROR) << "No profile type: " << network->profile_type();
    return;
  }

  if (network->type() == TYPE_WIFI) {
    WifiNetwork* remembered_wifi = new WifiNetwork(network->service_path());
    remembered_wifi->set_encryption(remembered_wifi->encryption());
    remembered = remembered_wifi;
  } else if (network->type() == TYPE_VPN) {
    VirtualNetwork* remembered_vpn =
        new VirtualNetwork(network->service_path());
    remembered_vpn->set_server_hostname("vpnserver.fake.com");
    remembered_vpn->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
    remembered = remembered_vpn;
  }
  if (remembered) {
    remembered->set_name(network->name());
    remembered->set_unique_id(network->unique_id());
    // AddRememberedNetwork will insert the network into the matching profile
    // and set the profile type + path.
    AddRememberedNetwork(remembered);
  }
}

void NetworkLibraryImplStub::ConnectToNetwork(Network* network) {
  // Set connected state.
  network->set_connected(true);
  network->set_connection_started(false);

  // Make the connected network the highest priority network.
  // Set all other networks of the same type to disconnected + inactive;
  int old_priority_order = network->priority_order_;
  network->priority_order_ = 0;
  for (NetworkMap::iterator iter = network_map_.begin();
       iter != network_map_.end(); ++iter) {
    Network* other = iter->second;
    if (other == network)
      continue;
    if (other->priority_order_ < old_priority_order)
      other->priority_order_++;
    if (other->type() == network->type()) {
      other->set_is_active(false);
      other->set_connected(false);
    }
  }

  // Remember connected network.
  if (network->profile_type() == PROFILE_NONE) {
    NetworkProfileType profile_type = PROFILE_USER;
    if (network->type() == TYPE_WIFI) {
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      if (!wifi->encrypted())
        profile_type = PROFILE_SHARED;
    }
    SetProfileType(network, profile_type);
  }
  AddStubRememberedNetwork(network);

  // Call Completed and signal observers.
  NetworkConnectCompleted(network, CONNECT_SUCCESS);
  SignalNetworkManagerObservers();
  NotifyNetworkChanged(network);
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase implementation.

void NetworkLibraryImplStub::CallConnectToNetwork(Network* network) {
  // Immediately set the network to active to mimic flimflam's behavior.
  SetActiveNetwork(network->type(), network->service_path());
  // If a delay has been set (i.e. we are interactive), delay the call to
  // ConnectToNetwork (but signal observers since we changed connecting state).
  if (connect_delay_ms_) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&NetworkLibraryImplStub::ConnectToNetwork,
                   base::Unretained(this), network),
        connect_delay_ms_);
    SignalNetworkManagerObservers();
    NotifyNetworkChanged(network);
  } else {
    ConnectToNetwork(network);
  }
}

void NetworkLibraryImplStub::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  WifiNetwork* wifi = new WifiNetwork(ssid);
  wifi->set_name(ssid);
  wifi->set_encryption(security);
  AddNetwork(wifi);
  ConnectToWifiNetworkUsingConnectData(wifi);
  SignalNetworkManagerObservers();
}

void NetworkLibraryImplStub::CallRequestVirtualNetworkAndConnect(
    const std::string& service_name,
    const std::string& server_hostname,
    ProviderType provider_type) {
  VirtualNetwork* vpn = new VirtualNetwork(service_name);
  vpn->set_name(service_name);
  vpn->set_server_hostname(server_hostname);
  vpn->set_provider_type(provider_type);
  AddNetwork(vpn);
  ConnectToVirtualNetworkUsingConnectData(vpn);
  SignalNetworkManagerObservers();
}

/////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplStub::ChangePin(const std::string& old_pin,
                                       const std::string& new_pin) {
  sim_operation_ = SIM_OPERATION_CHANGE_PIN;
  if (!pin_required_ || old_pin == pin_) {
    pin_ = new_pin;
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::ChangeRequirePin(bool require_pin,
                                              const std::string& pin) {
  sim_operation_ = SIM_OPERATION_CHANGE_REQUIRE_PIN;
  if (!pin_required_ || pin == pin_) {
    pin_required_ = require_pin;
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::EnterPin(const std::string& pin) {
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  if (!pin_required_ || pin == pin_) {
    pin_entered_ = true;
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  // TODO(stevenjb): something?
  NotifyPinOperationCompleted(PIN_ERROR_NONE);
}

void NetworkLibraryImplStub::RequestNetworkScan() {
  // This is triggered by user interaction, so set a network conenct delay.
  const int kConnectDelayMs = 4 * 1000;
  connect_delay_ms_ = kConnectDelayMs;
  SignalNetworkManagerObservers();
}

bool NetworkLibraryImplStub::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  *result = WifiAccessPointVector();
  return true;
}

void NetworkLibraryImplStub::DisconnectFromNetwork(const Network* network) {
  // Update the network state here since no network manager in stub impl.
  Network* modify_network = const_cast<Network*>(network);
  modify_network->set_is_active(false);
  modify_network->set_connected(false);
  if (network == active_wifi_)
    active_wifi_ = NULL;
  else if (network == active_cellular_)
    active_cellular_ = NULL;
  else if (network == active_virtual_)
    active_virtual_ = NULL;
  SignalNetworkManagerObservers();
  NotifyNetworkChanged(network);
}

NetworkIPConfigVector NetworkLibraryImplStub::GetIPConfigs(
    const std::string& device_path,
    std::string* hardware_address,
    HardwareAddressFormat format) {
  *hardware_address = hardware_address_;
  return ip_configs_;
}

void NetworkLibraryImplStub::SetIPConfig(const NetworkIPConfig& ipconfig) {
    ip_configs_.push_back(ipconfig);
}

/////////////////////////////////////////////////////////////////////////////

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

/////////////////////////////////////////////////////////////////////////////

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until its last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::NetworkLibraryImplBase);
