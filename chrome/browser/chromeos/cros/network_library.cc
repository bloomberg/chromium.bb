// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <algorithm>
#include <map>

#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/network_login_observer.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/common/time_format.h"
#include "content/browser/browser_thread.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

////////////////////////////////////////////////////////////////////////////////
// Implementation notes.
// NetworkLibraryImpl manages a series of classes that describe network devices
// and services:
//
// NetworkDevice: e.g. ethernet, wifi modem, cellular modem
//  device_map_: canonical map<name,NetworkDevice*> for devices
//
// Network: a network service ("network").
//  network_map_: canonical map<name,Network*> for all visible networks.
//  EthernetNetwork
//   ethernet_: EthernetNetwork* to the active ethernet network in network_map_.
//  WirelessNetwork: a Wifi or Cellular Network.
//  WifiNetwork
//   wifi_: WifiNetwork* to the active wifi network in network_map_.
//   wifi_networks_: ordered vector of WifiNetwork* entries in network_map_,
//       in descending order of importance.
//  CellularNetwork
//   cellular_: Cellular version of wifi_.
//   cellular_networks_: Cellular version of wifi_.
// remembered_network_map_: a canonical map<name,Network*> for all networks
//     remembered in the active Profile ("favorites").
// remembered_wifi_networks_: ordered vector of WifiNetwork* entries in
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
//  libcros (RequestNetworkServiceInfo).
//
// TODO(stevenjb): Document cellular data plan handlers.
//
// AddNetworkObserver: Adds an observer for a specific network.
// NetworkObserverList: A monitor and list of observers of a network.
// network_monitor_: a handle to the libcros network Service handler.
// UpdateNetworkStatus: This handles changes to a monitored service, typically
//     changes to transient states like Strength. (Note: also updates State).
//
////////////////////////////////////////////////////////////////////////////////

namespace chromeos {

// Local constants.
namespace {

// Only send network change notifications to observers once every 50ms.
const int kNetworkNotifyDelayMs = 50;

// How long we should remember that cellular plan payment was received.
const int kRecentPlanPaymentHours = 6;

// D-Bus interface string constants.

// Flimflam property names.
const char* kSecurityProperty = "Security";
const char* kPassphraseProperty = "Passphrase";
const char* kIdentityProperty = "Identity";
const char* kCertPathProperty = "CertPath";
const char* kPassphraseRequiredProperty = "PassphraseRequired";
const char* kProfilesProperty = "Profiles";
const char* kServicesProperty = "Services";
const char* kServiceWatchListProperty = "ServiceWatchList";
const char* kAvailableTechnologiesProperty = "AvailableTechnologies";
const char* kEnabledTechnologiesProperty = "EnabledTechnologies";
const char* kConnectedTechnologiesProperty = "ConnectedTechnologies";
const char* kDefaultTechnologyProperty = "DefaultTechnology";
const char* kOfflineModeProperty = "OfflineMode";
const char* kSignalStrengthProperty = "Strength";
const char* kNameProperty = "Name";
const char* kStateProperty = "State";
const char* kConnectivityStateProperty = "ConnectivityState";
const char* kTypeProperty = "Type";
const char* kDeviceProperty = "Device";
const char* kActivationStateProperty = "Cellular.ActivationState";
const char* kNetworkTechnologyProperty = "Cellular.NetworkTechnology";
const char* kRoamingStateProperty = "Cellular.RoamingState";
const char* kOperatorNameProperty = "Cellular.OperatorName";
const char* kOperatorCodeProperty = "Cellular.OperatorCode";
const char* kPaymentURLProperty = "Cellular.OlpUrl";
const char* kUsageURLProperty = "Cellular.UsageUrl";
const char* kFavoriteProperty = "Favorite";
const char* kConnectableProperty = "Connectable";
const char* kAutoConnectProperty = "AutoConnect";
const char* kIsActiveProperty = "IsActive";
const char* kModeProperty = "Mode";
const char* kErrorProperty = "Error";
const char* kActiveProfileProperty = "ActiveProfile";
const char* kEntriesProperty = "Entries";
const char* kDevicesProperty = "Devices";

// Flimflam device info property names.
const char* kScanningProperty = "Scanning";
const char* kCarrierProperty = "Cellular.Carrier";
const char* kMeidProperty = "Cellular.MEID";
const char* kImeiProperty = "Cellular.IMEI";
const char* kImsiProperty = "Cellular.IMSI";
const char* kEsnProperty = "Cellular.ESN";
const char* kMdnProperty = "Cellular.MDN";
const char* kMinProperty = "Cellular.MIN";
const char* kModelIDProperty = "Cellular.ModelID";
const char* kManufacturerProperty = "Cellular.Manufacturer";
const char* kFirmwareRevisionProperty = "Cellular.FirmwareRevision";
const char* kHardwareRevisionProperty = "Cellular.HardwareRevision";
const char* kLastDeviceUpdateProperty = "Cellular.LastDeviceUpdate";
const char* kPRLVersionProperty = "Cellular.PRLVersion"; // (INT16)

// Flimflam type options.
const char* kTypeEthernet = "ethernet";
const char* kTypeWifi = "wifi";
const char* kTypeWimax = "wimax";
const char* kTypeBluetooth = "bluetooth";
const char* kTypeCellular = "cellular";

// Flimflam mode options.
const char* kModeManaged = "managed";
const char* kModeAdhoc = "adhoc";

// Flimflam security options.
const char* kSecurityWpa = "wpa";
const char* kSecurityWep = "wep";
const char* kSecurityRsn = "rsn";
const char* kSecurity8021x = "802_1x";
const char* kSecurityNone = "none";

// Flimflam state options.
const char* kStateIdle = "idle";
const char* kStateCarrier = "carrier";
const char* kStateAssociation = "association";
const char* kStateConfiguration = "configuration";
const char* kStateReady = "ready";
const char* kStateDisconnect = "disconnect";
const char* kStateFailure = "failure";
const char* kStateActivationFailure = "activation-failure";

// Flimflam connectivity state options.
const char* kConnStateUnrestricted = "unrestricted";
const char* kConnStateRestricted = "restricted";
const char* kConnStateNone = "none";

// Flimflam network technology options.
const char* kNetworkTechnology1Xrtt = "1xRTT";
const char* kNetworkTechnologyEvdo = "EVDO";
const char* kNetworkTechnologyGprs = "GPRS";
const char* kNetworkTechnologyEdge = "EDGE";
const char* kNetworkTechnologyUmts = "UMTS";
const char* kNetworkTechnologyHspa = "HSPA";
const char* kNetworkTechnologyHspaPlus = "HSPA+";
const char* kNetworkTechnologyLte = "LTE";
const char* kNetworkTechnologyLteAdvanced = "LTE Advanced";

// Flimflam roaming state options
const char* kRoamingStateHome = "home";
const char* kRoamingStateRoaming = "roaming";
const char* kRoamingStateUnknown = "unknown";

// Flimflam activation state options
const char* kActivationStateActivated = "activated";
const char* kActivationStateActivating = "activating";
const char* kActivationStateNotActivated = "not-activated";
const char* kActivationStatePartiallyActivated = "partially-activated";
const char* kActivationStateUnknown = "unknown";

// Flimflam error options.
const char* kErrorOutOfRange = "out-of-range";
const char* kErrorPinMissing = "pin-missing";
const char* kErrorDhcpFailed = "dhcp-failed";
const char* kErrorConnectFailed = "connect-failed";
const char* kErrorBadPassphrase = "bad-passphrase";
const char* kErrorBadWEPKey = "bad-wepkey";
const char* kErrorActivationFailed = "activation-failed";
const char* kErrorNeedEvdo = "need-evdo";
const char* kErrorNeedHomeNetwork = "need-home-network";
const char* kErrorOtaspFailed = "otasp-failed";
const char* kErrorAaaFailed = "aaa-failed";

////////////////////////////////////////////////////////////////////////////

// Helper class to cache maps of strings to enums.
template <typename Type>
class StringToEnum {
 public:
  struct Pair {
    const char* key;
    const Type value;
  };

  explicit StringToEnum(const Pair* list, size_t num_entries, Type unknown)
      : unknown_value_(unknown) {
    for (size_t i = 0; i < num_entries; ++i, ++list)
      enum_map_[list->key] = list->value;
  }

  Type Get(const std::string& type) const {
    EnumMapConstIter iter = enum_map_.find(type);
    if (iter != enum_map_.end())
      return iter->second;
    return unknown_value_;
  }

 private:
  typedef typename std::map<std::string, Type> EnumMap;
  typedef typename std::map<std::string, Type>::const_iterator EnumMapConstIter;
  EnumMap enum_map_;
  Type unknown_value_;
  DISALLOW_COPY_AND_ASSIGN(StringToEnum);
};

////////////////////////////////////////////////////////////////////////////

enum PropertyIndex {
  PROPERTY_INDEX_ACTIVATION_STATE,
  PROPERTY_INDEX_ACTIVE_PROFILE,
  PROPERTY_INDEX_AUTO_CONNECT,
  PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES,
  PROPERTY_INDEX_CARRIER,
  PROPERTY_INDEX_CERT_PATH,
  PROPERTY_INDEX_CONNECTABLE,
  PROPERTY_INDEX_CONNECTED_TECHNOLOGIES,
  PROPERTY_INDEX_CONNECTIVITY_STATE,
  PROPERTY_INDEX_DEFAULT_TECHNOLOGY,
  PROPERTY_INDEX_DEVICE,
  PROPERTY_INDEX_DEVICES,
  PROPERTY_INDEX_ENABLED_TECHNOLOGIES,
  PROPERTY_INDEX_ERROR,
  PROPERTY_INDEX_ESN,
  PROPERTY_INDEX_FAVORITE,
  PROPERTY_INDEX_FIRMWARE_REVISION,
  PROPERTY_INDEX_HARDWARE_REVISION,
  PROPERTY_INDEX_IDENTITY,
  PROPERTY_INDEX_IMEI,
  PROPERTY_INDEX_IMSI,
  PROPERTY_INDEX_IS_ACTIVE,
  PROPERTY_INDEX_LAST_DEVICE_UPDATE,
  PROPERTY_INDEX_MANUFACTURER,
  PROPERTY_INDEX_MDN,
  PROPERTY_INDEX_MEID,
  PROPERTY_INDEX_MIN,
  PROPERTY_INDEX_MODE,
  PROPERTY_INDEX_MODEL_ID,
  PROPERTY_INDEX_NAME,
  PROPERTY_INDEX_NETWORK_TECHNOLOGY,
  PROPERTY_INDEX_OFFLINE_MODE,
  PROPERTY_INDEX_OPERATOR_CODE,
  PROPERTY_INDEX_OPERATOR_NAME,
  PROPERTY_INDEX_PASSPHRASE,
  PROPERTY_INDEX_PASSPHRASE_REQUIRED,
  PROPERTY_INDEX_PAYMENT_URL,
  PROPERTY_INDEX_PRL_VERSION,
  PROPERTY_INDEX_PROFILES,
  PROPERTY_INDEX_ROAMING_STATE,
  PROPERTY_INDEX_SCANNING,
  PROPERTY_INDEX_SECURITY,
  PROPERTY_INDEX_SERVICES,
  PROPERTY_INDEX_SERVICE_WATCH_LIST,
  PROPERTY_INDEX_SIGNAL_STRENGTH,
  PROPERTY_INDEX_STATE,
  PROPERTY_INDEX_TYPE,
  PROPERTY_INDEX_UNKNOWN,
  PROPERTY_INDEX_USAGE_URL,
};

StringToEnum<PropertyIndex>::Pair property_index_table[] = {
  { kActivationStateProperty, PROPERTY_INDEX_ACTIVATION_STATE },
  { kActiveProfileProperty, PROPERTY_INDEX_ACTIVE_PROFILE },
  { kAutoConnectProperty, PROPERTY_INDEX_AUTO_CONNECT },
  { kAvailableTechnologiesProperty, PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES },
  { kCarrierProperty, PROPERTY_INDEX_CARRIER },
  { kCertPathProperty, PROPERTY_INDEX_CERT_PATH },
  { kConnectableProperty, PROPERTY_INDEX_CONNECTABLE },
  { kConnectedTechnologiesProperty, PROPERTY_INDEX_CONNECTED_TECHNOLOGIES },
  { kConnectivityStateProperty, PROPERTY_INDEX_CONNECTIVITY_STATE },
  { kDefaultTechnologyProperty, PROPERTY_INDEX_DEFAULT_TECHNOLOGY },
  { kDeviceProperty, PROPERTY_INDEX_DEVICE },
  { kDevicesProperty, PROPERTY_INDEX_DEVICES },
  { kEnabledTechnologiesProperty, PROPERTY_INDEX_ENABLED_TECHNOLOGIES },
  { kErrorProperty, PROPERTY_INDEX_ERROR },
  { kEsnProperty, PROPERTY_INDEX_ESN },
  { kFavoriteProperty, PROPERTY_INDEX_FAVORITE },
  { kFirmwareRevisionProperty, PROPERTY_INDEX_FIRMWARE_REVISION },
  { kHardwareRevisionProperty, PROPERTY_INDEX_HARDWARE_REVISION },
  { kIdentityProperty, PROPERTY_INDEX_IDENTITY },
  { kImeiProperty, PROPERTY_INDEX_IMEI },
  { kImsiProperty, PROPERTY_INDEX_IMSI },
  { kIsActiveProperty, PROPERTY_INDEX_IS_ACTIVE },
  { kLastDeviceUpdateProperty, PROPERTY_INDEX_LAST_DEVICE_UPDATE },
  { kManufacturerProperty, PROPERTY_INDEX_MANUFACTURER },
  { kMdnProperty, PROPERTY_INDEX_MDN },
  { kMeidProperty, PROPERTY_INDEX_MEID },
  { kMinProperty, PROPERTY_INDEX_MIN },
  { kModeProperty, PROPERTY_INDEX_MODE },
  { kModelIDProperty, PROPERTY_INDEX_MODEL_ID },
  { kNameProperty, PROPERTY_INDEX_NAME },
  { kNetworkTechnologyProperty, PROPERTY_INDEX_NETWORK_TECHNOLOGY },
  { kOfflineModeProperty, PROPERTY_INDEX_OFFLINE_MODE },
  { kOperatorCodeProperty, PROPERTY_INDEX_OPERATOR_CODE },
  { kOperatorNameProperty, PROPERTY_INDEX_OPERATOR_NAME },
  { kPRLVersionProperty, PROPERTY_INDEX_PRL_VERSION },
  { kPassphraseProperty, PROPERTY_INDEX_PASSPHRASE },
  { kPassphraseRequiredProperty, PROPERTY_INDEX_PASSPHRASE_REQUIRED },
  { kPaymentURLProperty, PROPERTY_INDEX_PAYMENT_URL },
  { kProfilesProperty, PROPERTY_INDEX_PROFILES },
  { kRoamingStateProperty, PROPERTY_INDEX_ROAMING_STATE },
  { kScanningProperty, PROPERTY_INDEX_SCANNING },
  { kSecurityProperty, PROPERTY_INDEX_SECURITY },
  { kServiceWatchListProperty, PROPERTY_INDEX_SERVICE_WATCH_LIST },
  { kServicesProperty, PROPERTY_INDEX_SERVICES },
  { kSignalStrengthProperty, PROPERTY_INDEX_SIGNAL_STRENGTH },
  { kStateProperty, PROPERTY_INDEX_STATE },
  { kTypeProperty, PROPERTY_INDEX_TYPE },
  { kUsageURLProperty, PROPERTY_INDEX_USAGE_URL },
};

StringToEnum<PropertyIndex>& property_index_parser() {
  static StringToEnum<PropertyIndex> parser(property_index_table,
                                            arraysize(property_index_table),
                                            PROPERTY_INDEX_UNKNOWN);
  return parser;
}

////////////////////////////////////////////////////////////////////////////
// Parse strings from libcros.

// Network.
static ConnectionType ParseType(const std::string& type) {
  static StringToEnum<ConnectionType>::Pair table[] = {
    { kTypeEthernet, TYPE_ETHERNET },
    { kTypeWifi, TYPE_WIFI },
    { kTypeWimax, TYPE_WIMAX },
    { kTypeBluetooth, TYPE_BLUETOOTH },
    { kTypeCellular, TYPE_CELLULAR },
  };
  static StringToEnum<ConnectionType> parser(
      table, arraysize(table), TYPE_UNKNOWN);
  return parser.Get(type);
}

ConnectionType ParseTypeFromDictionary(const DictionaryValue* info) {
  std::string type_string;
  info->GetString(kTypeProperty, &type_string);
  return ParseType(type_string);
}

static ConnectionMode ParseMode(const std::string& mode) {
  static StringToEnum<ConnectionMode>::Pair table[] = {
    { kModeManaged, MODE_MANAGED },
    { kModeAdhoc, MODE_ADHOC },
  };
  static StringToEnum<ConnectionMode> parser(
      table, arraysize(table), MODE_UNKNOWN);
  return parser.Get(mode);
}

static ConnectionState ParseState(const std::string& state) {
  static StringToEnum<ConnectionState>::Pair table[] = {
    { kStateIdle, STATE_IDLE },
    { kStateCarrier, STATE_CARRIER },
    { kStateAssociation, STATE_ASSOCIATION },
    { kStateConfiguration, STATE_CONFIGURATION },
    { kStateReady, STATE_READY },
    { kStateDisconnect, STATE_DISCONNECT },
    { kStateFailure, STATE_FAILURE },
    { kStateActivationFailure, STATE_ACTIVATION_FAILURE },
  };
  static StringToEnum<ConnectionState> parser(
      table, arraysize(table), STATE_UNKNOWN);
  return parser.Get(state);
}

static ConnectionError ParseError(const std::string& error) {
  static StringToEnum<ConnectionError>::Pair table[] = {
    { kErrorOutOfRange, ERROR_OUT_OF_RANGE },
    { kErrorPinMissing, ERROR_PIN_MISSING },
    { kErrorDhcpFailed, ERROR_DHCP_FAILED },
    { kErrorConnectFailed, ERROR_CONNECT_FAILED },
    { kErrorBadPassphrase, ERROR_BAD_PASSPHRASE },
    { kErrorBadWEPKey, ERROR_BAD_WEPKEY },
    { kErrorActivationFailed, ERROR_ACTIVATION_FAILED },
    { kErrorNeedEvdo, ERROR_NEED_EVDO },
    { kErrorNeedHomeNetwork, ERROR_NEED_HOME_NETWORK },
    { kErrorOtaspFailed, ERROR_OTASP_FAILED },
    { kErrorAaaFailed, ERROR_AAA_FAILED },
  };
  static StringToEnum<ConnectionError> parser(
      table, arraysize(table), ERROR_UNKNOWN);
  return parser.Get(error);
}

// CellularNetwork.
static ActivationState ParseActivationState(const std::string& state) {
  static StringToEnum<ActivationState>::Pair table[] = {
    { kActivationStateActivated, ACTIVATION_STATE_ACTIVATED },
    { kActivationStateActivating, ACTIVATION_STATE_ACTIVATING },
    { kActivationStateNotActivated, ACTIVATION_STATE_NOT_ACTIVATED },
    { kActivationStatePartiallyActivated, ACTIVATION_STATE_PARTIALLY_ACTIVATED},
    { kActivationStateUnknown, ACTIVATION_STATE_UNKNOWN},
  };
  static StringToEnum<ActivationState> parser(
      table, arraysize(table), ACTIVATION_STATE_UNKNOWN);
  return parser.Get(state);
}

static ConnectivityState ParseConnectivityState(const std::string& state) {
  static StringToEnum<ConnectivityState>::Pair table[] = {
    { kConnStateUnrestricted, CONN_STATE_UNRESTRICTED },
    { kConnStateRestricted, CONN_STATE_RESTRICTED },
    { kConnStateNone, CONN_STATE_NONE },
  };
  static StringToEnum<ConnectivityState> parser(
      table, arraysize(table), CONN_STATE_UNKNOWN);
  return parser.Get(state);
}

static NetworkTechnology ParseNetworkTechnology(const std::string& technology) {
  static StringToEnum<NetworkTechnology>::Pair table[] = {
    { kNetworkTechnology1Xrtt, NETWORK_TECHNOLOGY_1XRTT },
    { kNetworkTechnologyEvdo, NETWORK_TECHNOLOGY_EVDO },
    { kNetworkTechnologyGprs, NETWORK_TECHNOLOGY_GPRS },
    { kNetworkTechnologyEdge, NETWORK_TECHNOLOGY_EDGE },
    { kNetworkTechnologyUmts, NETWORK_TECHNOLOGY_UMTS },
    { kNetworkTechnologyHspa, NETWORK_TECHNOLOGY_HSPA },
    { kNetworkTechnologyHspaPlus, NETWORK_TECHNOLOGY_HSPA_PLUS },
    { kNetworkTechnologyLte, NETWORK_TECHNOLOGY_LTE },
    { kNetworkTechnologyLteAdvanced, NETWORK_TECHNOLOGY_LTE_ADVANCED },
  };
  static StringToEnum<NetworkTechnology> parser(
      table, arraysize(table), NETWORK_TECHNOLOGY_UNKNOWN);
  return parser.Get(technology);
}

static NetworkRoamingState ParseRoamingState(const std::string& roaming_state) {
  static StringToEnum<NetworkRoamingState>::Pair table[] = {
    { kRoamingStateHome, ROAMING_STATE_HOME },
    { kRoamingStateRoaming, ROAMING_STATE_ROAMING },
    { kRoamingStateUnknown, ROAMING_STATE_UNKNOWN },
  };
  static StringToEnum<NetworkRoamingState> parser(
      table, arraysize(table), ROAMING_STATE_UNKNOWN);
  return parser.Get(roaming_state);
}

// WifiNetwork
static ConnectionSecurity ParseSecurity(const std::string& security) {
  static StringToEnum<ConnectionSecurity>::Pair table[] = {
    { kSecurity8021x, SECURITY_8021X },
    { kSecurityRsn, SECURITY_RSN },
    { kSecurityWpa, SECURITY_WPA },
    { kSecurityWep, SECURITY_WEP },
    { kSecurityNone, SECURITY_NONE },
  };
  static StringToEnum<ConnectionSecurity> parser(
      table, arraysize(table), SECURITY_UNKNOWN);
  return parser.Get(security);
}

////////////////////////////////////////////////////////////////////////////
// Html output helper functions

// Helper function to wrap Html with <th> tag.
static std::string WrapWithTH(std::string text) {
  return "<th>" + text + "</th>";
}

// Helper function to wrap Html with <td> tag.
static std::string WrapWithTD(std::string text) {
  return "<td>" + text + "</td>";
}

// Helper function to create an Html table header for a Network.
static std::string ToHtmlTableHeader(Network* network) {
  std::string str;
  if (network->type() == TYPE_ETHERNET) {
    str += WrapWithTH("Active");
  } else if (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR) {
    str += WrapWithTH("Name") + WrapWithTH("Active") +
        WrapWithTH("Auto-Connect") + WrapWithTH("Strength");
    if (network->type() == TYPE_WIFI)
      str += WrapWithTH("Encryption") + WrapWithTH("Passphrase") +
          WrapWithTH("Identity") + WrapWithTH("Certificate");
  }
  str += WrapWithTH("State") + WrapWithTH("Error") + WrapWithTH("IP Address");
  return str;
}

// Helper function to create an Html table row for a Network.
static std::string ToHtmlTableRow(Network* network) {
  std::string str;
  if (network->type() == TYPE_ETHERNET) {
    str += WrapWithTD(base::IntToString(network->is_active()));
  } else if (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR) {
    WirelessNetwork* wireless = static_cast<WirelessNetwork*>(network);
    str += WrapWithTD(wireless->name()) +
        WrapWithTD(base::IntToString(network->is_active())) +
        WrapWithTD(base::IntToString(wireless->auto_connect())) +
        WrapWithTD(base::IntToString(wireless->strength()));
    if (network->type() == TYPE_WIFI) {
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      str += WrapWithTD(wifi->GetEncryptionString()) +
          WrapWithTD(std::string(wifi->passphrase().length(), '*')) +
          WrapWithTD(wifi->identity()) + WrapWithTD(wifi->cert_path());
    }
  }
  str += WrapWithTD(network->GetStateString()) +
      WrapWithTD(network->failed() ? network->GetErrorString() : "") +
      WrapWithTD(network->ip_address());
  return str;
}

////////////////////////////////////////////////////////////////////////////////
// Misc.

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
static std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

static bool EnsureCrosLoaded() {
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    return false;
  } else {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "chromeos_library calls made from non UI thread!";
      NOTREACHED();
    }
    return true;
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NetworkDevice

NetworkDevice::NetworkDevice(const std::string& device_path)
    : device_path_(device_path),
      type_(TYPE_UNKNOWN),
      scanning_(false),
      PRL_version_(0) {
}

bool NetworkDevice::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        type_ = ParseType(type_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NAME:
      return value->GetAsString(&name_);
    case PROPERTY_INDEX_SCANNING:
      return value->GetAsBoolean(&scanning_);
    case PROPERTY_INDEX_CARRIER:
      return value->GetAsString(&carrier_);
    case PROPERTY_INDEX_MEID:
      return value->GetAsString(&MEID_);
    case PROPERTY_INDEX_IMEI:
      return value->GetAsString(&IMEI_);
    case PROPERTY_INDEX_IMSI:
      return value->GetAsString(&IMSI_);
    case PROPERTY_INDEX_ESN:
      return value->GetAsString(&ESN_);
    case PROPERTY_INDEX_MDN:
      return value->GetAsString(&MDN_);
    case PROPERTY_INDEX_MIN:
      return value->GetAsString(&MIN_);
    case PROPERTY_INDEX_MODEL_ID:
      return value->GetAsString(&model_id_);
    case PROPERTY_INDEX_MANUFACTURER:
      return value->GetAsString(&manufacturer_);
    case PROPERTY_INDEX_FIRMWARE_REVISION:
      return value->GetAsString(&firmware_revision_);
    case PROPERTY_INDEX_HARDWARE_REVISION:
      return value->GetAsString(&hardware_revision_);
    case PROPERTY_INDEX_LAST_DEVICE_UPDATE:
      return value->GetAsString(&last_update_);
    case PROPERTY_INDEX_PRL_VERSION:
      return value->GetAsInteger(&PRL_version_);
    default:
      break;
  }
  return false;
}

void NetworkDevice::ParseInfo(const DictionaryValue* info) {
  LOG(WARNING) << "Device: " << device_path_;
  for (DictionaryValue::key_iterator iter = info->begin_keys();
       iter != info->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = info->GetWithoutPathExpansion(key, &value);
    CHECK(res);
    int index = property_index_parser().Get(key);
    if (!ParseValue(index, value))
      VLOG(1) << "NetworkDevice: Unhandled key: " << key;
  }
}

////////////////////////////////////////////////////////////////////////////////
// Network

bool Network::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        ConnectionType type = ParseType(type_string);
        LOG_IF(ERROR, type != type_)
            << "Network with mismatched type: " << service_path_
            << " " << type << " != " << type_;
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_DEVICE:
      return value->GetAsString(&device_path_);
    case PROPERTY_INDEX_NAME:
      return value->GetAsString(&name_);
    case PROPERTY_INDEX_STATE: {
      std::string state_string;
      if (value->GetAsString(&state_string)) {
        ConnectionState prev_state = state_;
        state_ = ParseState(state_string);
        if (state_ != prev_state) {
          // State changed, so refresh IP address.
          // Note: blocking DBus call. TODO(stevenjb): refactor this.
          InitIPAddress();
          // If cellular state has just changed to connected request data plans.
          if (type_ == TYPE_CELLULAR && connected())
            RequestCellularDataPlanUpdate(service_path_.c_str());
        }
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MODE: {
      std::string mode_string;
      if (value->GetAsString(&mode_string)) {
        mode_ = ParseMode(mode_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ERROR: {
      std::string error_string;
      if (value->GetAsString(&error_string)) {
        error_ = ParseError(error_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CONNECTABLE:
      return value->GetAsBoolean(&connectable_);
    case PROPERTY_INDEX_IS_ACTIVE:
      return value->GetAsBoolean(&is_active_);
    case PROPERTY_INDEX_FAVORITE:
      return value->GetAsBoolean(&favorite_);
    case PROPERTY_INDEX_AUTO_CONNECT:
      return value->GetAsBoolean(&auto_connect_);
    case PROPERTY_INDEX_CONNECTIVITY_STATE: {
      std::string connectivity_state_string;
      if (value->GetAsString(&connectivity_state_string)) {
        connectivity_state_ = ParseConnectivityState(connectivity_state_string);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

void Network::ParseInfo(const DictionaryValue* info) {
  for (DictionaryValue::key_iterator iter = info->begin_keys();
       iter != info->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = info->GetWithoutPathExpansion(key, &value);
    CHECK(res);
    int index = property_index_parser().Get(key);
    if (!ParseValue(index, value))  // virtual.
      VLOG(1) << "Network: Type: " << type_ << " Unhandled key: " << key;
  }
}

// Used by GetHtmlInfo() which is called from the about:network handler.
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
    default:
      // Usually no default, but changes to libcros may add states.
      break;
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

std::string Network::GetErrorString() const {
  switch (error_) {
    case ERROR_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
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
    default:
      // Usually no default, but changes to libcros may add errors.
      break;
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  // If connected, get ip config.
  if (EnsureCrosLoaded() && connected() && !device_path_.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path_.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; i++) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        if (strlen(ipconfig.address) > 0) {
          ip_address_ = ipconfig.address;
          break;
        }
      }
      FreeIPConfigStatus(ipconfig_status);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// WirelessNetwork

bool WirelessNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_SIGNAL_STRENGTH:
      return value->GetAsInteger(&strength_);
    default:
      return Network::ParseValue(index, value);
      break;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CellularDataPlan

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
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
                base::TimeFormatFriendlyDate(plan_start_time));
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
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
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
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
// CellularNetwork

CellularNetwork::~CellularNetwork() {
}

bool CellularNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_ACTIVATION_STATE: {
      std::string activation_state_string;
      if (value->GetAsString(&activation_state_string)) {
        activation_state_ = ParseActivationState(activation_state_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NETWORK_TECHNOLOGY: {
      std::string network_technology_string;
      if (value->GetAsString(&network_technology_string)) {
        network_technology_ = ParseNetworkTechnology(network_technology_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ROAMING_STATE: {
      std::string roaming_state_string;
      if (value->GetAsString(&roaming_state_string)) {
        roaming_state_ = ParseRoamingState(roaming_state_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_OPERATOR_NAME:
      return value->GetAsString(&operator_name_);
    case PROPERTY_INDEX_OPERATOR_CODE:
      return value->GetAsString(&operator_code_);
    case PROPERTY_INDEX_PAYMENT_URL:
      return value->GetAsString(&payment_url_);
    case PROPERTY_INDEX_USAGE_URL:
      return value->GetAsString(&usage_url_);
    default:
      return WirelessNetwork::ParseValue(index, value);
  }
  return false;
}

bool CellularNetwork::StartActivation() const {
  if (!EnsureCrosLoaded())
    return false;
  return ActivateCellularModem(service_path().c_str(), NULL);
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
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_CELLULAR_TECHNOLOGY_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetConnectivityStateString() const {
  // These strings do not appear in the UI, so no need to localize them
  switch (connectivity_state_) {
    case CONN_STATE_UNRESTRICTED:
      return "unrestricted";
      break;
    case CONN_STATE_RESTRICTED:
      return "restricted";
      break;
    case CONN_STATE_NONE:
      return "none";
      break;
    case CONN_STATE_UNKNOWN:
    default:
      return "unknown";
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

bool WifiNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_SECURITY: {
      std::string security_string;
      if (value->GetAsString(&security_string)) {
        encryption_ = ParseSecurity(security_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PASSPHRASE:
      return value->GetAsString(&passphrase_);
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED:
      return value->GetAsBoolean(&passphrase_required_);
    case PROPERTY_INDEX_IDENTITY:
      return value->GetAsString(&identity_);
    case PROPERTY_INDEX_CERT_PATH:
      return value->GetAsString(&cert_path_);
    default:
      return WirelessNetwork::ParseValue(index, value);
  }
  return false;
}

std::string WifiNetwork::GetEncryptionString() {
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
    case SECURITY_8021X:
      return "8021X";
  }
  return "Unknown";
}

bool WifiNetwork::IsPassphraseRequired() const {
  // TODO(stevenjb): Remove error_ tests when fixed in flimflam
  // (http://crosbug.com/10135).
  if (error_ == ERROR_BAD_PASSPHRASE || error_ == ERROR_BAD_WEPKEY)
    return true;
  return passphrase_required_;
}

// Parse 'path' to determine if the certificate is stored in a pkcs#11 device.
// flimflam recognizes the string "SETTINGS:" to specify authentication
// parameters. 'key_id=' indicates that the certificate is stored in a pkcs#11
// device. See src/third_party/flimflam/files/doc/service-api.txt.
bool WifiNetwork::IsCertificateLoaded() const {
  static const std::string settings_string("SETTINGS:");
  static const std::string pkcs11_key("key_id");
  if (cert_path_.find(settings_string) == 0) {
    std::string::size_type idx = cert_path_.find(pkcs11_key);
    if (idx != std::string::npos)
      idx = cert_path_.find_first_not_of(kWhitespaceASCII,
                                         idx + pkcs11_key.length());
    if (idx != std::string::npos && cert_path_[idx] == '=')
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

class NetworkLibraryImpl : public NetworkLibrary  {
 public:
  NetworkLibraryImpl()
      : network_manager_monitor_(NULL),
        data_plan_monitor_(NULL),
        ethernet_(NULL),
        wifi_(NULL),
        cellular_(NULL),
        available_devices_(0),
        enabled_devices_(0),
        connected_devices_(0),
        wifi_scanning_(false),
        offline_mode_(false),
        is_locked_(false),
        notify_task_(NULL) {
    if (EnsureCrosLoaded()) {
      Init();
      network_manager_monitor_ =
          MonitorNetworkManager(&NetworkManagerStatusChangedHandler,
                                this);
      data_plan_monitor_ = MonitorCellularDataPlan(&DataPlanUpdateHandler,
                                                   this);
      network_login_observer_.reset(new NetworkLoginObserver(this));
    } else {
      InitTestData();
    }
  }

  virtual ~NetworkLibraryImpl() {
    network_manager_observers_.Clear();
    if (network_manager_monitor_)
      DisconnectPropertyChangeMonitor(network_manager_monitor_);
    data_plan_observers_.Clear();
    if (data_plan_monitor_)
      DisconnectDataPlanUpdateMonitor(data_plan_monitor_);
    STLDeleteValues(&network_observers_);
    ClearNetworks(true /*delete networks*/);
    ClearRememberedNetworks(true /*delete networks*/);
    STLDeleteValues(&data_plan_map_);
  }

  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {
    if (!network_manager_observers_.HasObserver(observer))
      network_manager_observers_.AddObserver(observer);
  }

  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {
    network_manager_observers_.RemoveObserver(observer);
  }

  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {
    DCHECK(observer);
    if (!EnsureCrosLoaded())
      return;
    // First, add the observer to the callback map.
    NetworkObserverMap::iterator iter = network_observers_.find(service_path);
    NetworkObserverList* oblist;
    if (iter != network_observers_.end()) {
      oblist = iter->second;
    } else {
      std::pair<NetworkObserverMap::iterator, bool> inserted =
        network_observers_.insert(
            std::make_pair<std::string, NetworkObserverList*>(
                service_path,
                new NetworkObserverList(this, service_path)));
      oblist = inserted.first->second;
    }
    if (!oblist->HasObserver(observer))
      oblist->AddObserver(observer);
  }

  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {
    DCHECK(observer);
    DCHECK(service_path.size());
    NetworkObserverMap::iterator map_iter =
        network_observers_.find(service_path);
    if (map_iter != network_observers_.end()) {
      map_iter->second->RemoveObserver(observer);
      if (!map_iter->second->size()) {
        delete map_iter->second;
        network_observers_.erase(map_iter++);
      }
    }
  }

  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {
    DCHECK(observer);
    NetworkObserverMap::iterator map_iter = network_observers_.begin();
    while (map_iter != network_observers_.end()) {
      map_iter->second->RemoveObserver(observer);
      if (!map_iter->second->size()) {
        delete map_iter->second;
        network_observers_.erase(map_iter++);
      } else {
        ++map_iter;
      }
    }
  }

  virtual void Lock() {
    if (is_locked_)
      return;
    is_locked_ = true;
    NotifyNetworkManagerChanged();
  }

  virtual void Unlock() {
    DCHECK(is_locked_);
    if (!is_locked_)
      return;
    is_locked_ = false;
    NotifyNetworkManagerChanged();
  }

  virtual bool IsLocked() {
    return is_locked_;
  }

  virtual void AddCellularDataPlanObserver(CellularDataPlanObserver* observer) {
    if (!data_plan_observers_.HasObserver(observer))
      data_plan_observers_.AddObserver(observer);
  }

  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {
    data_plan_observers_.RemoveObserver(observer);
  }

  virtual void AddUserActionObserver(UserActionObserver* observer) {
    if (!user_action_observers_.HasObserver(observer))
      user_action_observers_.AddObserver(observer);
  }

  virtual void RemoveUserActionObserver(UserActionObserver* observer) {
    user_action_observers_.RemoveObserver(observer);
  }

  virtual const EthernetNetwork* ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const {
    return ethernet_ ? ethernet_->connecting() : false;
  }
  virtual bool ethernet_connected() const {
    return ethernet_ ? ethernet_->connected() : false;
  }

  virtual const WifiNetwork* wifi_network() const { return wifi_; }
  virtual bool wifi_connecting() const {
    return wifi_ ? wifi_->connecting() : false;
  }
  virtual bool wifi_connected() const {
    return wifi_ ? wifi_->connected() : false;
  }

  virtual const CellularNetwork* cellular_network() const { return cellular_; }
  virtual bool cellular_connecting() const {
    return cellular_ ? cellular_->connecting() : false;
  }
  virtual bool cellular_connected() const {
    return cellular_ ? cellular_->connected() : false;
  }

  bool Connected() const {
    return ethernet_connected() || wifi_connected() || cellular_connected();
  }

  bool Connecting() const {
    return ethernet_connecting() || wifi_connecting() || cellular_connecting();
  }

  const std::string& IPAddress() const {
    // Returns IP address for the active network.
    const Network* active = active_network();
    if (active != NULL)
      return active->ip_address();
    if (ethernet_)
      return ethernet_->ip_address();
    static std::string null_address("0.0.0.0");
    return null_address;
  }

  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return remembered_wifi_networks_;
  }

  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  /////////////////////////////////////////////////////////////////////////////

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const {
    NetworkDeviceMap::const_iterator iter = device_map_.find(path);
    if (iter != device_map_.end())
      return iter->second;
    LOG(WARNING) << "Device path not found: " << path;
    return NULL;
  }

  virtual const WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const {
    return GetWifiNetworkByPath(path);
  }

  virtual const CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const {
    return GetCellularNetworkByPath(path);
  }

  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const {
    CellularDataPlanMap::const_iterator iter = data_plan_map_.find(path);
    if (iter != data_plan_map_.end())
      return iter->second;
    return NULL;
  }

  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const {
    const CellularDataPlanVector* plans = GetDataPlans(path);
    if (plans)
      return GetSignificantDataPlanFromVector(plans);
    return NULL;
  }

  virtual void RequestWifiScan() {
    if (EnsureCrosLoaded() && wifi_enabled()) {
      wifi_scanning_ = true;  // Cleared when updates are received.
      RequestScan(TYPE_WIFI);
      RequestRememberedNetworksUpdate();
    }
  }

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    if (!EnsureCrosLoaded())
      return false;
    DeviceNetworkList* network_list = GetDeviceNetworkList();
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
    FreeDeviceNetworkList(network_list);
    return true;
  }

  bool CallConnectToNetworkForWifi(const std::string& service_path,
                                   const std::string& password,
                                   const std::string& identity,
                                   const std::string& certpath) {
    // Blocking DBus call. TODO(stevenjb): make async.
    if (ConnectToNetworkWithCertInfo(
            service_path.c_str(),
            password.empty() ? NULL : password.c_str(),
            identity.empty() ? NULL : identity.c_str(),
            certpath.empty() ? NULL : certpath.c_str())) {
      // Update local cache and notify listeners.
      WifiNetwork* wifi = GetWifiNetworkByPath(service_path);
      if (wifi) {
        wifi->set_passphrase(password);
        wifi->set_identity(identity);
        wifi->set_cert_path(certpath);
        wifi->set_connecting(true);
        wifi_ = wifi;
      }
      // If we succeed, this network will be remembered; request an update.
      // TODO(stevenjb): flimflam should do this automatically.
      RequestRememberedNetworksUpdate();
      // Notify observers.
      NotifyNetworkManagerChanged();
      NotifyUserConnectionInitated(wifi);
      return true;
    }
    return false;
  }

  // Use this when the code needs to cache a copy of the WifiNetwork and
  // expects |network|->error_ to be set on failure.
  virtual bool ConnectToWifiNetwork(WifiNetwork* network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return true;  // No library loaded, don't trigger a retry attempt.
    bool res = CallConnectToNetworkForWifi(
        network->service_path(), password, identity, certpath);
    if (!res) {
      // The only likely cause for an immediate failure is a badly formatted
      // passphrase. TODO(stevenjb): get error information from libcros
      // and call set_error correctly. crosbug.com/9538.
      // NOTE: This only sets the error field of |network|, which will
      // always be a local copy, not the network_library copy.
      // The network_library copy will be updated by libcros.
      network->set_error(ERROR_BAD_PASSPHRASE);
    }
    return res;
  }

  // Use this to connect to a wifi network by service path.
  virtual bool ConnectToWifiNetwork(const std::string& service_path,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    if (!EnsureCrosLoaded())
      return true;  // No library loaded, don't trigger a retry attempt.
    WifiNetwork* wifi = GetWifiNetworkByPath(service_path);
    if (!wifi) {
      LOG(WARNING) << "Attempt to connect to non existing network: "
                   << service_path;
      return false;
    }
    bool res = CallConnectToNetworkForWifi(
        service_path, password, identity, certpath);
    return res;
  }

  // Use this to connect to an unlisted wifi network.
  // This needs to request information about the named service.
  // The connection attempt will occur in the callback.
  virtual bool ConnectToWifiNetwork(ConnectionSecurity security,
                                    const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) {
    if (!EnsureCrosLoaded())
      return true;  // No library loaded, don't trigger a retry attempt.
    RequestWifiServicePath(ssid.c_str(),
                           security,
                           WifiServiceUpdateAndConnect,
                           this);
    // Store the connection data to be used by the callback.
    connect_data_.SetData(ssid, password, identity, certpath, auto_connect);
    return true;  // No immediate failure mode
  }

  // Callback
  static void WifiServiceUpdateAndConnect(void* object,
                                          const char* service_path,
                                          const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path && info) {
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      Network* network =
          networklib->ParseNetwork(std::string(service_path), dict);
      // Blocking DBus call. TODO(stevenjb): make async.
      networklib->ConnectToNetworkUsingConnectData(network);
    }
  }

  void ConnectToNetworkUsingConnectData(Network* network) {
    ConnectData& data = connect_data_;
    if (network->name() != data.name) {
      LOG(WARNING) << "Network name does not match ConnectData: "
                   << network->name() << " != " << data.name;
      return;
    }
    // Blocking DBus call. TODO(stevenjb): make async.
    if (ConnectToNetworkWithCertInfo(
            network->service_path().c_str(),
            data.password.empty() ? NULL : data.password.c_str(),
            data.identity.empty() ? NULL : data.identity.c_str(),
            data.certpath.empty() ? NULL : data.certpath.c_str())) {
      LOG(WARNING) << "Connected to: " << network->service_path();
      // Set auto-connect (synchronous libcros method).
      SetAutoConnect(network->service_path().c_str(), data.auto_connect);
      // If we succeed, this network will be remembered; request an update.
      // TODO(stevenjb): flimflam should do this automatically.
      RequestRememberedNetworksUpdate();
      // Notify observers.
      NotifyNetworkManagerChanged();
      NotifyUserConnectionInitated(network);
    }
  }

  virtual bool ConnectToCellularNetwork(const CellularNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return true;  // No library loaded, don't trigger a retry attempt.
    // Blocking DBus call. TODO(stevenjb): make async.
    if (ConnectToNetwork(network->service_path().c_str(), NULL)) {
      // Update local cache and notify listeners.
      CellularNetwork* cellular =
          GetCellularNetworkByPath(network->service_path());
      if (cellular) {
        cellular->set_connecting(true);
        cellular_ = cellular;
      }
      NotifyNetworkManagerChanged();
      NotifyUserConnectionInitated(cellular);
      return true;
    } else {
      return false;  // Immediate failure.
    }
  }

  virtual void RefreshCellularDataPlans(const CellularNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    VLOG(1) << " Requesting data plan for: " << network->service_path();
    RequestCellularDataPlanUpdate(network->service_path().c_str());
  }

  // Records information that cellular play payment had happened.
  virtual void SignalCellularPlanPayment() {
    DCHECK(!HasRecentCellularPlanPayment());
    cellular_plan_payment_time_ = base::Time::Now();
  }

  // Returns true if cellular plan payment had been recorded recently.
  virtual bool HasRecentCellularPlanPayment() {
    return (base::Time::Now() -
            cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
  }

  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    if (DisconnectFromNetwork(network->service_path().c_str())) {
      // Update local cache and notify listeners.
      WirelessNetwork* wireless =
          GetWirelessNetworkByPath(network->service_path());
      if (wireless) {
        wireless->set_connected(false);
        if (wireless == wifi_)
          wifi_ = NULL;
        else if (wireless == cellular_)
          cellular_ = NULL;
      }
      NotifyNetworkManagerChanged();
    }
  }

  virtual void SaveCellularNetwork(const CellularNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    CellularNetwork* cellular =
        GetCellularNetworkByPath(network->service_path());
    if (!cellular) {
      LOG(WARNING) << "Save to unknown network: " << network->service_path();
      return;
    }

    // Immediately update properties in the cached structure.
    cellular->set_auto_connect(network->auto_connect());
    // Update libcros (synchronous).
    SetAutoConnect(network->service_path().c_str(), network->auto_connect());
  }

  virtual void SaveWifiNetwork(const WifiNetwork* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    WifiNetwork* wifi = GetWifiNetworkByPath(network->service_path());
    if (!wifi) {
      LOG(WARNING) << "Save to unknown network: " << network->service_path();
      return;
    }
    // Immediately update properties in the cached structure.
    wifi->set_passphrase(network->passphrase());
    wifi->set_identity(network->identity());
    wifi->set_cert_path(network->cert_path());
    wifi->set_auto_connect(network->auto_connect());
    // Update libcros (synchronous methods).
    const char* service_path = network->service_path().c_str();
    SetPassphrase(service_path, network->passphrase().c_str());
    SetIdentity(service_path, network->identity().c_str());
    SetCertPath(service_path, network->cert_path().c_str());
    SetAutoConnect(service_path, network->auto_connect());
  }

  virtual void SetNetworkAutoConnect(const std::string& service_path,
                                     bool auto_connect) {
    if (!EnsureCrosLoaded())
      return;
    WirelessNetwork* wireless = GetWirelessNetworkByPath(service_path);
    if (!wireless) {
      LOG(WARNING) << "Attempt to set autoconnect on network: " << service_path;
      return;
    }
    // Update libcros (synchronous).
    SetAutoConnect(service_path.c_str(), auto_connect);
    // Immediately update properties in the cached structure.
    wireless->set_auto_connect(auto_connect);
  }

  virtual void ForgetWifiNetwork(const std::string& service_path) {
    if (!EnsureCrosLoaded())
      return;
    // NOTE: service paths for remembered wifi networks do not match the
    // service paths in wifi_networks_; calling a libcros funtion that
    // operates on the wifi_networks_ list with this service_path will
    // trigger a crash because the DBUS path does not exist.
    // TODO(stevenjb): modify libcros to warn and fail instead of crash.
    // https://crosbug.com/9295
    if (DeleteRememberedService(service_path.c_str())) {
      DeleteRememberedNetwork(service_path);
      NotifyNetworkManagerChanged();
    }
  }

  virtual bool ethernet_available() const {
    return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const {
    return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const {
    return available_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool ethernet_enabled() const {
    return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const {
    return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const {
    return enabled_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool wifi_scanning() const {
    return wifi_scanning_;
  }

  virtual bool offline_mode() const { return offline_mode_; }

  virtual const Network* active_network() const {
    if (ethernet_ && ethernet_->is_active())
      return ethernet_;
    if (wifi_ && wifi_->is_active())
      return wifi_;
    if (cellular_ && cellular_->is_active())
      return cellular_;
    return NULL;
  }

  virtual void EnableEthernetNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_ETHERNET, enable);
  }

  virtual void EnableWifiNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_WIFI, enable);
  }

  virtual void EnableCellularNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_CELLULAR, enable);
  }

  virtual void EnableOfflineMode(bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && offline_mode_) {
      VLOG(1) << "Trying to enable offline mode when it's already enabled.";
      return;
    }
    if (!enable && !offline_mode_) {
      VLOG(1) << "Trying to disable offline mode when it's already disabled.";
      return;
    }

    if (SetOfflineMode(enable)) {
      offline_mode_ = enable;
    }
  }

  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address) {
    hardware_address->clear();
    NetworkIPConfigVector ipconfig_vector;
    if (EnsureCrosLoaded() && !device_path.empty()) {
      IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
      if (ipconfig_status) {
        for (int i = 0; i < ipconfig_status->size; i++) {
          IPConfig ipconfig = ipconfig_status->ips[i];
          ipconfig_vector.push_back(
              NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                              ipconfig.netmask, ipconfig.gateway,
                              ipconfig.name_servers));
        }
        *hardware_address = ipconfig_status->hardware_address;
        FreeIPConfigStatus(ipconfig_status);
        // Sort the list of ip configs by type.
        std::sort(ipconfig_vector.begin(), ipconfig_vector.end());
      }
    }
    return ipconfig_vector;
  }

  virtual std::string GetHtmlInfo(int refresh) {
    std::string output;
    output.append("<html><head><title>About Network</title>");
    if (refresh > 0)
      output.append("<meta http-equiv=\"refresh\" content=\"" +
          base::IntToString(refresh) + "\"/>");
    output.append("</head><body>");
    if (refresh > 0) {
      output.append("(Auto-refreshing page every " +
                    base::IntToString(refresh) + "s)");
    } else {
      output.append("(To auto-refresh this page: about:network/&lt;secs&gt;)");
    }

    if (ethernet_enabled()) {
      output.append("<h3>Ethernet:</h3><table border=1>");
      if (ethernet_) {
        output.append("<tr>" + ToHtmlTableHeader(ethernet_) + "</tr>");
        output.append("<tr>" + ToHtmlTableRow(ethernet_) + "</tr>");
      }
    }

    if (wifi_enabled()) {
      output.append("</table><h3>Wifi:</h3><table border=1>");
      for (size_t i = 0; i < wifi_networks_.size(); ++i) {
        if (i == 0)
          output.append("<tr>" + ToHtmlTableHeader(wifi_networks_[i]) +
                        "</tr>");
        output.append("<tr>" + ToHtmlTableRow(wifi_networks_[i]) + "</tr>");
      }
    }

    if (cellular_enabled()) {
      output.append("</table><h3>Cellular:</h3><table border=1>");
      for (size_t i = 0; i < cellular_networks_.size(); ++i) {
        if (i == 0)
          output.append("<tr>" + ToHtmlTableHeader(cellular_networks_[i]) +
                        "</tr>");
        output.append("<tr>" + ToHtmlTableRow(cellular_networks_[i]) + "</tr>");
      }
    }

    output.append("</table><h3>Remembered Wifi:</h3><table border=1>");
    for (size_t i = 0; i < remembered_wifi_networks_.size(); ++i) {
      if (i == 0)
        output.append(
            "<tr>" + ToHtmlTableHeader(remembered_wifi_networks_[i]) +
            "</tr>");
      output.append("<tr>" + ToHtmlTableRow(remembered_wifi_networks_[i]) +
          "</tr>");
    }

    output.append("</table></body></html>");
    return output;
  }

 private:

  typedef std::map<std::string, Network*> NetworkMap;
  typedef std::map<std::string, NetworkDevice*> NetworkDeviceMap;
  typedef std::map<std::string, CellularDataPlanVector*> CellularDataPlanMap;

  class NetworkObserverList : public ObserverList<NetworkObserver> {
   public:
    NetworkObserverList(NetworkLibraryImpl* library,
                        const std::string& service_path) {
      network_monitor_ = MonitorNetworkService(&NetworkStatusChangedHandler,
                                               service_path.c_str(),
                                               library);
    }

    virtual ~NetworkObserverList() {
      if (network_monitor_)
        DisconnectPropertyChangeMonitor(network_monitor_);
    }

   private:
    static void NetworkStatusChangedHandler(void* object,
                                            const char* path,
                                            const char* key,
                                            const Value* value) {
      NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
      DCHECK(networklib);
      networklib->UpdateNetworkStatus(path, key, value);
    }
    PropertyChangeMonitor network_monitor_;
  };

  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  ////////////////////////////////////////////////////////////////////////////
  // Callbacks.

  static void NetworkManagerStatusChangedHandler(void* object,
                                                 const char* path,
                                                 const char* key,
                                                 const Value* value) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    networklib->NetworkManagerStatusChanged(key, value);
  }

  // This processes all Manager update messages.
  void NetworkManagerStatusChanged(const char* key, const Value* value) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::TimeTicks start = base::TimeTicks::Now();
    VLOG(1) << "NetworkManagerStatusChanged: KEY=" << key;
    if (!key)
      return;
    int index = property_index_parser().Get(std::string(key));
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
        NotifyNetworkManagerChanged();
        break;
      }
      case PROPERTY_INDEX_ACTIVE_PROFILE: {
        DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
        value->GetAsString(&active_profile_path_);
        RequestRememberedNetworksUpdate();
        break;
      }
      case PROPERTY_INDEX_PROFILES:
        // Currently we ignore Profiles (list of all profiles).
        break;
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
      default:
        LOG(WARNING) << "Unhandled key: " << key;
        break;
    }
    base::TimeDelta delta = base::TimeTicks::Now() - start;
    VLOG(1) << "  time: " << delta.InMilliseconds() << " ms.";
    HISTOGRAM_TIMES("CROS_NETWORK_UPDATE", delta);
  }

  static void NetworkManagerUpdate(void* object,
                                   const char* manager_path,
                                   const Value* info) {
    VLOG(1) << "Received NetworkManagerUpdate.";
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    DCHECK(info);
    DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
    networklib->ParseNetworkManager(dict);
  }

  void ParseNetworkManager(const DictionaryValue* dict) {
    for (DictionaryValue::key_iterator iter = dict->begin_keys();
         iter != dict->end_keys(); ++iter) {
      const std::string& key = *iter;
      Value* value;
      bool res = dict->GetWithoutPathExpansion(key, &value);
      CHECK(res);
      NetworkManagerStatusChanged(key.c_str(), value);
    }
  }

  static void ProfileUpdate(void* object,
                            const char* profile_path,
                            const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    DCHECK(info);
    DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
    ListValue* entries(NULL);
    dict->GetList(kEntriesProperty, &entries);
    DCHECK(entries);
    networklib->UpdateRememberedServiceList(profile_path, entries);
  }

  static void NetworkServiceUpdate(void* object,
                                   const char* service_path,
                                   const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path) {
      if (!info) {
        // Network no longer exists.
        networklib->DeleteNetwork(std::string(service_path));
      } else {
        DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
        const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
        networklib->ParseNetwork(std::string(service_path), dict);
      }
    }
  }

  static void RememberedNetworkServiceUpdate(void* object,
                                             const char* service_path,
                                             const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path) {
      if (!info) {
        // Remembered network no longer exists.
        networklib->DeleteRememberedNetwork(std::string(service_path));
      } else {
        DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
        const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
        networklib->ParseRememberedNetwork(std::string(service_path), dict);
      }
    }
  }

  static void NetworkDeviceUpdate(void* object,
                                  const char* device_path,
                                  const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (device_path) {
      if (!info) {
        // device no longer exists.
        networklib->DeleteDevice(std::string(device_path));
      } else {
        DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
        const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
        networklib->ParseNetworkDevice(std::string(device_path), dict);
      }
    }
  }

  static void DataPlanUpdateHandler(void* object,
                                    const char* modem_service_path,
                                    const CellularDataPlanList* dataplan) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (modem_service_path && dataplan) {
      networklib->UpdateCellularDataPlan(std::string(modem_service_path),
                                         dataplan);
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // Network technology functions.

  void UpdateTechnologies(const ListValue* technologies, int* bitfieldp) {
    DCHECK(bitfieldp);
    if (!technologies)
      return;
    int bitfield = 0;
    for (ListValue::const_iterator iter = technologies->begin();
         iter != technologies->end(); ++iter) {
      std::string technology;
      (*iter)->GetAsString(&technology);
      if (!technology.empty()) {
        ConnectionType type = ParseType(technology);
        bitfield |= 1 << type;
      }
    }
    *bitfieldp = bitfield;
    NotifyNetworkManagerChanged();
  }

  void UpdateAvailableTechnologies(const ListValue* technologies) {
    UpdateTechnologies(technologies, &available_devices_);
  }

  void UpdateEnabledTechnologies(const ListValue* technologies) {
    UpdateTechnologies(technologies, &enabled_devices_);
    if (!ethernet_enabled())
      ethernet_ = NULL;
    if (!wifi_enabled()) {
      wifi_ = NULL;
      wifi_networks_.clear();
    }
    if (!cellular_enabled()) {
      cellular_ = NULL;
      cellular_networks_.clear();
    }
  }

  void UpdateConnectedTechnologies(const ListValue* technologies) {
    UpdateTechnologies(technologies, &connected_devices_);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Network list management functions.

  // Note: sometimes flimflam still returns networks when the device type is
  // disabled. Always check the appropriate enabled() state before adding
  // networks to a list or setting an active network so that we do not show them
  // in the UI.

  // This relies on services being requested from flimflam in priority order,
  // and the updates getting processed and received in order.
  void UpdateActiveNetwork(Network* network) {
    ConnectionType type(network->type());
    if (type == TYPE_ETHERNET) {
      if (ethernet_enabled()) {
        // Set ethernet_ to the first connected ethernet service, or the first
        // disconnected ethernet service if none are connected.
        if (ethernet_ == NULL || !ethernet_->connected())
          ethernet_ = static_cast<EthernetNetwork*>(network);
      }
    } else if (type == TYPE_WIFI) {
      if (wifi_enabled()) {
        // Set wifi_ to the first connected or connecting wifi service.
        if (wifi_ == NULL && network->connecting_or_connected())
          wifi_ = static_cast<WifiNetwork*>(network);
      }
    } else if (type == TYPE_CELLULAR) {
      if (cellular_enabled()) {
        // Set cellular_ to the first connected or connecting celluar service.
        if (cellular_ == NULL && network->connecting_or_connected())
          cellular_ = static_cast<CellularNetwork*>(network);
      }
    }
  }

  void AddNetwork(Network* network) {
    std::pair<NetworkMap::iterator,bool> result =
        network_map_.insert(std::make_pair(network->service_path(), network));
    DCHECK(result.second);  // Should only get called with new network.
    ConnectionType type(network->type());
    if (type == TYPE_WIFI) {
      if (wifi_enabled())
        wifi_networks_.push_back(static_cast<WifiNetwork*>(network));
    } else if (type == TYPE_CELLULAR) {
      if (cellular_enabled())
        cellular_networks_.push_back(static_cast<CellularNetwork*>(network));
    }
    // Do not set the active network here. Wait until we parse the network.
  }

  // This only gets called when NetworkServiceUpdate receives a NULL update
  // for an existing network, e.g. an error occurred while fetching a network.
  void DeleteNetwork(const std::string& service_path) {
    NetworkMap::iterator found = network_map_.find(service_path);
    if (found == network_map_.end()) {
      // This occurs when we receive an update request followed by a disconnect
      // which triggers another update. See UpdateNetworkServiceList.
      return;
    }
    Network* network = found->second;
    network_map_.erase(found);
    ConnectionType type(network->type());
    if (type == TYPE_ETHERNET) {
      if (network == ethernet_) {
        // This should never happen.
        LOG(ERROR) << "Deleting active ethernet network: " << service_path;
        ethernet_ = NULL;
      }
    } else if (type == TYPE_WIFI) {
      WifiNetworkVector::iterator iter = std::find(
          wifi_networks_.begin(), wifi_networks_.end(), network);
      if (iter != wifi_networks_.end())
        wifi_networks_.erase(iter);
      if (network == wifi_) {
        // This should never happen.
        LOG(ERROR) << "Deleting active wifi network: " << service_path;
        wifi_ = NULL;
      }
    } else if (type == TYPE_CELLULAR) {
      CellularNetworkVector::iterator iter = std::find(
          cellular_networks_.begin(), cellular_networks_.end(), network);
      if (iter != cellular_networks_.end())
        cellular_networks_.erase(iter);
      if (network == cellular_) {
        // This should never happen.
        LOG(ERROR) << "Deleting active cellular network: " << service_path;
        cellular_ = NULL;
      }
      // Find and delete any existing data plans associated with |service_path|.
      CellularDataPlanMap::iterator found =  data_plan_map_.find(service_path);
      if (found != data_plan_map_.end()) {
        CellularDataPlanVector* data_plans = found->second;
        delete data_plans;
        data_plan_map_.erase(found);
      }
    }
    delete network;
  }

  void AddRememberedNetwork(Network* network) {
    std::pair<NetworkMap::iterator,bool> result =
        remembered_network_map_.insert(
            std::make_pair(network->service_path(), network));
    DCHECK(result.second);  // Should only get called with new network.
    if (network->type() == TYPE_WIFI) {
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      remembered_wifi_networks_.push_back(wifi);
    }
  }

  void DeleteRememberedNetwork(const std::string& service_path) {
    NetworkMap::iterator found = remembered_network_map_.find(service_path);
    if (found == remembered_network_map_.end()) {
      LOG(WARNING) << "Attempt to delete non-existant remembered network: "
                   << service_path;
      return;
    }
    Network* remembered_network = found->second;
    remembered_network_map_.erase(found);
    WifiNetworkVector::iterator iter = std::find(
        remembered_wifi_networks_.begin(), remembered_wifi_networks_.end(),
        remembered_network);
    if (iter != remembered_wifi_networks_.end())
      remembered_wifi_networks_.erase(iter);
    delete remembered_network;
  }

  // Update all network lists, and request associated service updates.
  void UpdateNetworkServiceList(const ListValue* services) {
    // TODO(stevenjb): Wait for wifi_scanning_ to be false.
    // Copy the list of existing networks to "old" and clear the map and lists.
    NetworkMap old_network_map = network_map_;
    ClearNetworks(false /*don't delete*/);
    // Clear the list of update requests.
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
        network_update_requests_.insert(service_path);
        wifi_scanning_ = true;
        RequestNetworkServiceInfo(service_path.c_str(),
                                  &NetworkServiceUpdate,
                                  this);
      }
    }
    // Delete any old networks that no longer exist.
    for (NetworkMap::iterator iter = old_network_map.begin();
         iter != old_network_map.end(); ++iter) {
      delete iter->second;
    }
  }

  // Request updates for watched networks. Does not affect network lists.
  // Existing networks will be updated. There should not be any new networks
  // in this list, but if there are they will be added appropriately.
  void UpdateWatchedNetworkServiceList(const ListValue* services) {
    for (ListValue::const_iterator iter = services->begin();
         iter != services->end(); ++iter) {
      std::string service_path;
      (*iter)->GetAsString(&service_path);
      if (!service_path.empty()) {
        VLOG(1) << "Watched Service: " << service_path;
        RequestNetworkServiceInfo(
            service_path.c_str(), &NetworkServiceUpdate, this);
      }
    }
  }

  // Request the active profile which lists the remembered networks.
  void RequestRememberedNetworksUpdate() {
    RequestNetworkProfile(
        active_profile_path_.c_str(), &ProfileUpdate, this);
  }

  // Update the list of remembered (profile) networks, and request associated
  // service updates.
  void UpdateRememberedServiceList(const char* profile_path,
                                   const ListValue* profile_entries) {
    // Copy the list of existing networks to "old" and clear the map and list.
    NetworkMap old_network_map = remembered_network_map_;
    ClearRememberedNetworks(false /*don't delete*/);
    // |profile_entries| represents a complete list of remembered networks.
    for (ListValue::const_iterator iter = profile_entries->begin();
         iter != profile_entries->end(); ++iter) {
      std::string service_path;
      (*iter)->GetAsString(&service_path);
      if (!service_path.empty()) {
        // If we find the network in "old", add it immediately to the map
        // and list. Otherwise it will get added when
        // RememberedNetworkServiceUpdate calls ParseRememberedNetwork.
        NetworkMap::iterator found = old_network_map.find(service_path);
        if (found != old_network_map.end()) {
          AddRememberedNetwork(found->second);
          old_network_map.erase(found);
        }
        // Always request updates for remembered networks.
        RequestNetworkProfileEntry(profile_path,
                                   service_path.c_str(),
                                   &RememberedNetworkServiceUpdate,
                                   this);
      }
    }
    // Delete any old networks that no longer exist.
    for (NetworkMap::iterator iter = old_network_map.begin();
         iter != old_network_map.end(); ++iter) {
      delete iter->second;
    }
  }

  Network* CreateNewNetwork(ConnectionType type,
                            const std::string& service_path) {
    if (type == TYPE_ETHERNET) {
      EthernetNetwork* ethernet = new EthernetNetwork(service_path);
      return ethernet;
    } else if (type == TYPE_WIFI) {
      WifiNetwork* wifi = new WifiNetwork(service_path);
      return wifi;
    } else if (type == TYPE_CELLULAR) {
      CellularNetwork* cellular = new CellularNetwork(service_path);
      return cellular;
    } else {
      LOG(WARNING) << "Unknown service type: " << type;
      return new Network(service_path, type);
    }
  }

  Network* ParseNetwork(const std::string& service_path,
                        const DictionaryValue* info) {
    Network* network;
    NetworkMap::iterator found = network_map_.find(service_path);
    if (found != network_map_.end()) {
      network = found->second;
    } else {
      ConnectionType type = ParseTypeFromDictionary(info);
      network = CreateNewNetwork(type, service_path);
      AddNetwork(network);
    }
    network_update_requests_.erase(service_path);
    if (network_update_requests_.empty()) {
      // Clear wifi_scanning_ when we have no pending requests.
      wifi_scanning_ = false;
    }

    network->ParseInfo(info);  // virtual.

    UpdateActiveNetwork(network);

    VLOG(1) << "ParseNetwork:" << network->name();
    NotifyNetworkManagerChanged();
    return network;
  }

  Network* ParseRememberedNetwork(const std::string& service_path,
                                  const DictionaryValue* info) {
    Network* network;
    NetworkMap::iterator found = remembered_network_map_.find(service_path);
    if (found != remembered_network_map_.end()) {
      network = found->second;
    } else {
      ConnectionType type = ParseTypeFromDictionary(info);
      network = CreateNewNetwork(type, service_path);
      AddRememberedNetwork(network);
    }
    network->ParseInfo(info);  // virtual.
    VLOG(1) << "ParseRememberedNetwork:" << network->name();
    NotifyNetworkManagerChanged();
    return network;
  }

  void ClearNetworks(bool delete_networks) {
    if (delete_networks)
      STLDeleteValues(&network_map_);
    network_map_.clear();
    ethernet_ = NULL;
    wifi_ = NULL;
    cellular_ = NULL;
    wifi_networks_.clear();
    cellular_networks_.clear();
  }

  void ClearRememberedNetworks(bool delete_networks) {
    if (delete_networks)
      STLDeleteValues(&remembered_network_map_);
    remembered_network_map_.clear();
    remembered_wifi_networks_.clear();
  }

  ////////////////////////////////////////////////////////////////////////////
  // NetworkDevice list management functions.

  // Update device list, and request associated device updates.
  // |devices| represents a complete list of devices.
  void UpdateNetworkDeviceList(const ListValue* devices) {
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
          VLOG(2) << " Adding device: " << device_path;
          device_map_[device_path] = found->second;
          old_device_map.erase(found);
        }
        RequestNetworkDeviceInfo(
            device_path.c_str(), &NetworkDeviceUpdate, this);
      }
    }
    // Delete any old devices that no longer exist.
    for (NetworkDeviceMap::iterator iter = old_device_map.begin();
         iter != old_device_map.end(); ++iter) {
      delete iter->second;
    }
  }

  void DeleteDevice(const std::string& device_path) {
    NetworkDeviceMap::iterator found = device_map_.find(device_path);
    if (found == device_map_.end()) {
      LOG(WARNING) << "Attempt to delete non-existant device: "
                   << device_path;
      return;
    }
    VLOG(2) << " Deleting device: " << device_path;
    NetworkDevice* device = found->second;
    device_map_.erase(found);
    delete device;
  }

  void ParseNetworkDevice(const std::string& device_path,
                          const DictionaryValue* info) {
    NetworkDeviceMap::iterator found = device_map_.find(device_path);
    NetworkDevice* device;
    if (found != device_map_.end()) {
      device = found->second;
    } else {
      device = new NetworkDevice(device_path);
      VLOG(2) << " Adding device: " << device_path;
      device_map_[device_path] = device;
    }
    device->ParseInfo(info);
    VLOG(1) << "ParseNetworkDevice:" << device->name();
    NotifyNetworkManagerChanged();
  }

  ////////////////////////////////////////////////////////////////////////////

  WirelessNetwork* GetWirelessNetworkByPath(const std::string& path) const {
    NetworkMap::const_iterator iter = network_map_.find(path);
    if (iter != network_map_.end()) {
      Network* network = iter->second;
      if (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR)
        return static_cast<WirelessNetwork*>(network);
    }
    return NULL;
  }

  WifiNetwork* GetWifiNetworkByPath(const std::string& path) const {
    WirelessNetwork* network = GetWirelessNetworkByPath(path);
    if (network && network->type() == TYPE_WIFI)
      return static_cast<WifiNetwork*>(network);
    return NULL;
  }

  CellularNetwork* GetCellularNetworkByPath(const std::string& path) const {
    WirelessNetwork* network = GetWirelessNetworkByPath(path);
    if (network && network->type() == TYPE_CELLULAR)
      return static_cast<CellularNetwork*>(network);
    return NULL;
  }

  void EnableNetworkDeviceType(ConnectionType device, bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && (enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to enable a device that's already enabled: "
                   << device;
      return;
    }
    if (!enable && !(enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to disable a device that's already disabled: "
                   << device;
      return;
    }

    EnableNetworkDevice(device, enable);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Notifications.

  // We call this any time something in NetworkLibrary changes.
  // TODO(stevenjb): We should consider breaking this into multiplie
  // notifications, e.g. connection state, devices, services, etc.
  void NotifyNetworkManagerChanged() {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Limit the frequency of notifications.
    if (notify_task_)
      notify_task_->Cancel();
    notify_task_ = NewRunnableMethod(
        this, &NetworkLibraryImpl::SignalNetworkManagerObservers);
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, notify_task_,
                                   kNetworkNotifyDelayMs);
  }

  void SignalNetworkManagerObservers() {
    notify_task_ = NULL;
    FOR_EACH_OBSERVER(NetworkManagerObserver,
                      network_manager_observers_,
                      OnNetworkManagerChanged(this));
  }

  void NotifyNetworkChanged(Network* network) {
    DCHECK(network);
    NetworkObserverMap::const_iterator iter = network_observers_.find(
        network->service_path());
    if (iter != network_observers_.end()) {
      FOR_EACH_OBSERVER(NetworkObserver,
                        *(iter->second),
                        OnNetworkChanged(this, network));
    } else {
      NOTREACHED() <<
          "There weren't supposed to be any property change observers of " <<
           network->service_path();
    }
  }

  void NotifyCellularDataPlanChanged() {
    FOR_EACH_OBSERVER(CellularDataPlanObserver,
                      data_plan_observers_,
                      OnCellularDataPlanChanged(this));
  }

  void NotifyUserConnectionInitated(const Network* network) {
    FOR_EACH_OBSERVER(UserActionObserver,
                      user_action_observers_,
                      OnConnectionInitiated(this, network));
  }

  ////////////////////////////////////////////////////////////////////////////
  // Service updates.

  void UpdateNetworkStatus(const char* path,
                           const char* key,
                           const Value* value) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (key == NULL || value == NULL)
      return;
    NetworkMap::iterator iter = network_map_.find(path);
    if (iter != network_map_.end()) {
      VLOG(1) << "UpdateNetworkStatus: " << path << "." << key;
      Network* network = iter->second;
      // Note: ParseValue is virtual.
      int index = property_index_parser().Get(std::string(key));
      if (!network->ParseValue(index, value)) {
        LOG(WARNING) << "UpdateNetworkStatus: Error parsing: "
                     << path << "." << key;
      }
      NotifyNetworkChanged(network);
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // Data Plans.

  const CellularDataPlan* GetSignificantDataPlanFromVector(
      const CellularDataPlanVector* plans) const {
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

  CellularNetwork::DataLeft GetDataLeft(
      CellularDataPlanVector* data_plans) {
    const CellularDataPlan* plan = GetSignificantDataPlanFromVector(data_plans);
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

  void UpdateCellularDataPlan(const std::string& service_path,
                              const CellularDataPlanList* data_plan_list) {
    CellularDataPlanVector* data_plans = NULL;
    // Find and delete any existing data plans associated with |service_path|.
    CellularDataPlanMap::iterator found = data_plan_map_.find(service_path);
    if (found != data_plan_map_.end()) {
      data_plans = found->second;
      data_plans->reset();  // This will delete existing data plans.
    } else {
      data_plans = new CellularDataPlanVector;
      data_plan_map_[service_path] = data_plans;
    }
    for (size_t i = 0; i < data_plan_list->plans_size; i++) {
      const CellularDataPlanInfo* info(data_plan_list->GetCellularDataPlan(i));
      data_plans->push_back(new CellularDataPlan(*info));
    }
    // Now, update any matching cellular network's cached data
    CellularNetwork* cellular = GetCellularNetworkByPath(service_path);
    if (cellular) {
      CellularNetwork::DataLeft data_left;
      // If the network needs a new plan, then there's no data.
      if (cellular->needs_new_plan())
        data_left = CellularNetwork::DATA_NONE;
      else
        data_left = GetDataLeft(data_plans);
      cellular->set_data_left(data_left);
    }
    NotifyCellularDataPlanChanged();
  }

  ////////////////////////////////////////////////////////////////////////////

  void Init() {
    // First, get the currently available networks. This data is cached
    // on the connman side, so the call should be quick.
    if (EnsureCrosLoaded()) {
      VLOG(1) << "Requesting initial network manager info from libcros.";
      RequestNetworkManagerInfo(&NetworkManagerUpdate, this);
    }
  }

  void InitTestData() {
    is_locked_ = true;

    // Devices
    int devices =
        (1 << TYPE_ETHERNET) | (1 << TYPE_WIFI) | (1 << TYPE_CELLULAR);
    available_devices_ = devices;
    enabled_devices_ = devices;
    connected_devices_ = devices;

    // Networks
    ClearNetworks(true /*delete networks*/);

    ethernet_ = new EthernetNetwork("eth1");
    ethernet_->set_connected(true);
    AddNetwork(ethernet_);

    WifiNetwork* wifi1 = new WifiNetwork("fw1");
    wifi1->set_name("Fake Wifi 1");
    wifi1->set_strength(90);
    wifi1->set_connected(false);
    wifi1->set_encryption(SECURITY_NONE);
    AddNetwork(wifi1);

    WifiNetwork* wifi2 = new WifiNetwork("fw2");
    wifi2->set_name("Fake Wifi 2");
    wifi2->set_strength(70);
    wifi2->set_connected(true);
    wifi2->set_encryption(SECURITY_WEP);
    AddNetwork(wifi2);

    WifiNetwork* wifi3 = new WifiNetwork("fw3");
    wifi3->set_name("Fake Wifi 3");
    wifi3->set_strength(50);
    wifi3->set_connected(false);
    wifi3->set_encryption(SECURITY_8021X);
    wifi3->set_identity("nobody@google.com");
    wifi3->set_cert_path("SETTINGS:key_id=3,cert_id=3,pin=111111");
    AddNetwork(wifi3);

    wifi_ = wifi2;

    CellularNetwork* cellular1 = new CellularNetwork("fc1");
    cellular1->set_name("Fake Cellular 1");
    cellular1->set_strength(70);
    cellular1->set_connected(false);
    cellular1->set_activation_state(ACTIVATION_STATE_ACTIVATED);
    cellular1->set_payment_url(std::string("http://www.google.com"));
    cellular1->set_usage_url(std::string("http://www.google.com"));
    cellular1->set_network_technology(NETWORK_TECHNOLOGY_EVDO);

    CellularDataPlan* base_plan = new CellularDataPlan();
    base_plan->plan_name = "Base plan";
    base_plan->plan_type = CELLULAR_DATA_PLAN_METERED_BASE;
    base_plan->plan_data_bytes = 100ll * 1024 * 1024;
    base_plan->data_bytes_used = 75ll * 1024 * 1024;
    CellularDataPlanVector* data_plans = new CellularDataPlanVector();
    data_plan_map_[cellular1->service_path()] = data_plans;
    data_plans->push_back(base_plan);

    CellularDataPlan* paid_plan = new CellularDataPlan();
    paid_plan->plan_name = "Paid plan";
    paid_plan->plan_type = CELLULAR_DATA_PLAN_METERED_PAID;
    paid_plan->plan_data_bytes = 5ll * 1024 * 1024 * 1024;
    paid_plan->data_bytes_used = 3ll * 1024 * 1024 * 1024;
    data_plans->push_back(paid_plan);

    AddNetwork(cellular1);
    cellular_ = cellular1;

    // Remembered Networks
    ClearRememberedNetworks(true /*delete networks*/);
    WifiNetwork* remembered_wifi2 = new WifiNetwork("fw2");
    remembered_wifi2->set_name("Fake Wifi 2");
    remembered_wifi2->set_strength(70);
    remembered_wifi2->set_connected(true);
    remembered_wifi2->set_encryption(SECURITY_WEP);
    AddRememberedNetwork(remembered_wifi2);

    wifi_scanning_ = false;
    offline_mode_ = false;
  }

  // Network manager observer list
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // User action observer list
  ObserverList<UserActionObserver> user_action_observers_;

  // Network observer map
  NetworkObserverMap network_observers_;

  // For monitoring network manager status changes.
  PropertyChangeMonitor network_manager_monitor_;

  // For monitoring data plan changes to the connected cellular network.
  DataPlanUpdateMonitor data_plan_monitor_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

  // A service path based map of all Networks.
  NetworkMap network_map_;

  // A service path based map of all remembered Networks.
  NetworkMap remembered_network_map_;

  // A list of services that we are awaiting updates for.
  std::set<std::string> network_update_requests_;

  // A device path based map of all NetworkDevices.
  NetworkDeviceMap device_map_;

  // A network service path based map of all CellularDataPlanVectors.
  CellularDataPlanMap data_plan_map_;

  // The ethernet network.
  EthernetNetwork* ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork* wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork* cellular_;

  // The path of the active profile (for retrieving remembered services).
  std::string active_profile_path_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  // True if we are currently scanning for wifi networks.
  bool wifi_scanning_;

  // Currently not implemented. TODO: implement or eliminate.
  bool offline_mode_;

  // True if access network library is locked.
  bool is_locked_;

  // Delayed task to notify a network change.
  CancelableTask* notify_task_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  // Temporary connection data for async connect calls.
  struct ConnectData {
    ConnectData() : auto_connect(false) {}
    void SetData(const std::string& n,
                 const std::string& p,
                 const std::string& id,
                 const std::string& cert,
                 bool autocon) {
      name = n;
      password = p;
      identity = id;
      certpath = cert;
      auto_connect = autocon;
    }
    std::string name;
    std::string password;
    std::string identity;
    std::string certpath;
    bool auto_connect;
  };
  ConnectData connect_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImpl);
};

class NetworkLibraryStubImpl : public NetworkLibrary {
 public:
  NetworkLibraryStubImpl()
      : ip_address_("1.1.1.1"),
        ethernet_(new EthernetNetwork("eth0")),
        wifi_(NULL),
        cellular_(NULL) {
  }
  ~NetworkLibraryStubImpl() { if (ethernet_) delete ethernet_; }
  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {}
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {}
  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {}
  virtual void Lock() {}
  virtual void Unlock() {}
  virtual bool IsLocked() { return true; }
  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual void AddUserActionObserver(UserActionObserver* observer) {}
  virtual void RemoveUserActionObserver(UserActionObserver* observer) {}
  virtual const EthernetNetwork* ethernet_network() const {
    return ethernet_;
  }
  virtual bool ethernet_connecting() const { return false; }
  virtual bool ethernet_connected() const { return true; }
  virtual const WifiNetwork* wifi_network() const {
    return wifi_;
  }
  virtual bool wifi_connecting() const { return false; }
  virtual bool wifi_connected() const { return false; }
  virtual const CellularNetwork* cellular_network() const {
    return cellular_;
  }
  virtual bool cellular_connecting() const { return false; }
  virtual bool cellular_connected() const { return false; }

  bool Connected() const { return true; }
  bool Connecting() const { return false; }
  const std::string& IPAddress() const { return ip_address_; }
  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }
  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return wifi_networks_;
  }
  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }
  virtual bool has_cellular_networks() const {
    return cellular_networks_.begin() != cellular_networks_.end();
  }
  /////////////////////////////////////////////////////////////////////////////

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const { return NULL; }
  virtual const WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual const CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const { return NULL; }
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const { return NULL; }

  virtual void RequestWifiScan() {}
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    return false;
  }

  virtual bool ConnectToWifiNetwork(WifiNetwork* network,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    return true;
  }
  virtual bool ConnectToWifiNetwork(const std::string& service_path,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath) {
    return true;
  }
  virtual bool ConnectToWifiNetwork(ConnectionSecurity security,
                                    const std::string& ssid,
                                    const std::string& password,
                                    const std::string& identity,
                                    const std::string& certpath,
                                    bool auto_connect) {
    return true;
  }
  virtual bool ConnectToCellularNetwork(const CellularNetwork* network) {
    return true;
  }
  virtual void RefreshCellularDataPlans(const CellularNetwork* network) {}
  virtual void SignalCellularPlanPayment() {}
  virtual bool HasRecentCellularPlanPayment() { return false; }
  virtual void DisconnectFromWirelessNetwork(const WirelessNetwork* network) {}
  virtual void SaveCellularNetwork(const CellularNetwork* network) {}
  virtual void SaveWifiNetwork(const WifiNetwork* network) {}
  virtual void SetNetworkAutoConnect(const std::string& service_path,
                                     bool auto_connect) {}
  virtual void ForgetWifiNetwork(const std::string& service_path) {}
  virtual bool ethernet_available() const { return true; }
  virtual bool wifi_available() const { return false; }
  virtual bool cellular_available() const { return false; }
  virtual bool ethernet_enabled() const { return true; }
  virtual bool wifi_enabled() const { return false; }
  virtual bool cellular_enabled() const { return false; }
  virtual bool wifi_scanning() const { return false; }
  virtual const Network* active_network() const { return NULL; }
  virtual bool offline_mode() const { return false; }
  virtual void EnableEthernetNetworkDevice(bool enable) {}
  virtual void EnableWifiNetworkDevice(bool enable) {}
  virtual void EnableCellularNetworkDevice(bool enable) {}
  virtual void EnableOfflineMode(bool enable) {}
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address) {
    hardware_address->clear();
    return NetworkIPConfigVector();
  }
  virtual std::string GetHtmlInfo(int refresh) { return std::string(); }

 private:
  std::string ip_address_;
  EthernetNetwork* ethernet_;
  WifiNetwork* wifi_;
  CellularNetwork* cellular_;
  WifiNetworkVector wifi_networks_;
  CellularNetworkVector cellular_networks_;
};

// static
NetworkLibrary* NetworkLibrary::GetImpl(bool stub) {
  if (stub)
    return new NetworkLibraryStubImpl();
  else
    return new NetworkLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::NetworkLibraryImpl);
