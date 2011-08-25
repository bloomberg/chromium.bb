// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/native_network_parser.h"

#include "base/stringprintf.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"

namespace chromeos {

// Local constants.
namespace {

EnumMapper<PropertyIndex>::Pair property_index_table[] = {
  { kActivationStateProperty, PROPERTY_INDEX_ACTIVATION_STATE },
  { kActiveProfileProperty, PROPERTY_INDEX_ACTIVE_PROFILE },
  { kArpGatewayProperty, PROPERTY_INDEX_ARP_GATEWAY },
  { kAutoConnectProperty, PROPERTY_INDEX_AUTO_CONNECT },
  { kAvailableTechnologiesProperty, PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES },
  { kCarrierProperty, PROPERTY_INDEX_CARRIER },
  { kCellularAllowRoamingProperty, PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING },
  { kCellularApnListProperty, PROPERTY_INDEX_CELLULAR_APN_LIST },
  { kCellularApnProperty, PROPERTY_INDEX_CELLULAR_APN },
  { kCellularLastGoodApnProperty, PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN },
  { kCheckPortalListProperty, PROPERTY_INDEX_CHECK_PORTAL_LIST },
  { kConnectableProperty, PROPERTY_INDEX_CONNECTABLE },
  { kConnectedTechnologiesProperty, PROPERTY_INDEX_CONNECTED_TECHNOLOGIES },
  { kDefaultTechnologyProperty, PROPERTY_INDEX_DEFAULT_TECHNOLOGY },
  { kDeviceProperty, PROPERTY_INDEX_DEVICE },
  { kDevicesProperty, PROPERTY_INDEX_DEVICES },
  { kEapAnonymousIdentityProperty, PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY },
  { kEapCaCertIdProperty, PROPERTY_INDEX_EAP_CA_CERT_ID },
  { kEapCaCertNssProperty, PROPERTY_INDEX_EAP_CA_CERT_NSS },
  { kEapCaCertProperty, PROPERTY_INDEX_EAP_CA_CERT },
  { kEapCertIdProperty, PROPERTY_INDEX_EAP_CERT_ID },
  { kEapClientCertNssProperty, PROPERTY_INDEX_EAP_CLIENT_CERT_NSS },
  { kEapClientCertProperty, PROPERTY_INDEX_EAP_CLIENT_CERT },
  { kEapIdentityProperty, PROPERTY_INDEX_EAP_IDENTITY },
  { kEapKeyIdProperty, PROPERTY_INDEX_EAP_KEY_ID },
  { kEapKeyMgmtProperty, PROPERTY_INDEX_EAP_KEY_MGMT },
  { kEapMethodProperty, PROPERTY_INDEX_EAP_METHOD },
  { kEapPasswordProperty, PROPERTY_INDEX_EAP_PASSWORD },
  { kEapPhase2AuthProperty, PROPERTY_INDEX_EAP_PHASE_2_AUTH },
  { kEapPinProperty, PROPERTY_INDEX_EAP_PIN },
  { kEapPrivateKeyPasswordProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD },
  { kEapPrivateKeyProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY },
  { kEapUseSystemCasProperty, PROPERTY_INDEX_EAP_USE_SYSTEM_CAS },
  { kEnabledTechnologiesProperty, PROPERTY_INDEX_ENABLED_TECHNOLOGIES },
  { kErrorProperty, PROPERTY_INDEX_ERROR },
  { kEsnProperty, PROPERTY_INDEX_ESN },
  { kFavoriteProperty, PROPERTY_INDEX_FAVORITE },
  { kFirmwareRevisionProperty, PROPERTY_INDEX_FIRMWARE_REVISION },
  { kFoundNetworksProperty, PROPERTY_INDEX_FOUND_NETWORKS },
  { kGuidProperty, PROPERTY_INDEX_GUID },
  { kHardwareRevisionProperty, PROPERTY_INDEX_HARDWARE_REVISION },
  { kHomeProviderProperty, PROPERTY_INDEX_HOME_PROVIDER },
  { kHostProperty, PROPERTY_INDEX_HOST },
  { kIdentityProperty, PROPERTY_INDEX_IDENTITY },
  { kImeiProperty, PROPERTY_INDEX_IMEI },
  { kImsiProperty, PROPERTY_INDEX_IMSI },
  { kIsActiveProperty, PROPERTY_INDEX_IS_ACTIVE },
  { kL2tpIpsecCaCertNssProperty, PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS },
  { kL2tpIpsecClientCertIdProperty, PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID },
  { kL2tpIpsecClientCertSlotProp, PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT },
  { kL2tpIpsecPinProperty, PROPERTY_INDEX_L2TPIPSEC_PIN },
  { kL2tpIpsecPskProperty, PROPERTY_INDEX_L2TPIPSEC_PSK },
  { kL2tpIpsecPasswordProperty, PROPERTY_INDEX_L2TPIPSEC_PASSWORD },
  { kL2tpIpsecUserProperty, PROPERTY_INDEX_L2TPIPSEC_USER },
  { kManufacturerProperty, PROPERTY_INDEX_MANUFACTURER },
  { kMdnProperty, PROPERTY_INDEX_MDN },
  { kMeidProperty, PROPERTY_INDEX_MEID },
  { kMinProperty, PROPERTY_INDEX_MIN },
  { kModeProperty, PROPERTY_INDEX_MODE },
  { kModelIdProperty, PROPERTY_INDEX_MODEL_ID },
  { kNameProperty, PROPERTY_INDEX_NAME },
  { kNetworkTechnologyProperty, PROPERTY_INDEX_NETWORK_TECHNOLOGY },
  { kNetworksProperty, PROPERTY_INDEX_NETWORKS },
  { kOfflineModeProperty, PROPERTY_INDEX_OFFLINE_MODE },
  { kOperatorCodeProperty, PROPERTY_INDEX_OPERATOR_CODE },
  { kOperatorNameProperty, PROPERTY_INDEX_OPERATOR_NAME },
  { kPrlVersionProperty, PROPERTY_INDEX_PRL_VERSION },
  { kPassphraseProperty, PROPERTY_INDEX_PASSPHRASE },
  { kPassphraseRequiredProperty, PROPERTY_INDEX_PASSPHRASE_REQUIRED },
  { kPaymentUrlProperty, PROPERTY_INDEX_PAYMENT_URL },
  { kPortalUrlProperty, PROPERTY_INDEX_PORTAL_URL },
  { kPoweredProperty, PROPERTY_INDEX_POWERED },
  { kPriorityProperty, PROPERTY_INDEX_PRIORITY },
  { kProfileProperty, PROPERTY_INDEX_PROFILE },
  { kProfilesProperty, PROPERTY_INDEX_PROFILES },
  { kProviderProperty, PROPERTY_INDEX_PROVIDER },
  { kProxyConfigProperty, PROPERTY_INDEX_PROXY_CONFIG },
  { kRoamingStateProperty, PROPERTY_INDEX_ROAMING_STATE },
  { kSimLockStatusProperty, PROPERTY_INDEX_SIM_LOCK },
  { kSaveCredentialsProperty, PROPERTY_INDEX_SAVE_CREDENTIALS },
  { kScanningProperty, PROPERTY_INDEX_SCANNING },
  { kSecurityProperty, PROPERTY_INDEX_SECURITY },
  { kSelectedNetworkProperty, PROPERTY_INDEX_SELECTED_NETWORK },
  { kServiceWatchListProperty, PROPERTY_INDEX_SERVICE_WATCH_LIST },
  { kServicesProperty, PROPERTY_INDEX_SERVICES },
  { kServingOperatorProperty, PROPERTY_INDEX_SERVING_OPERATOR },
  { kSignalStrengthProperty, PROPERTY_INDEX_SIGNAL_STRENGTH },
  { kStateProperty, PROPERTY_INDEX_STATE },
  { kSupportNetworkScanProperty, PROPERTY_INDEX_SUPPORT_NETWORK_SCAN },
  { kTechnologyFamilyProperty, PROPERTY_INDEX_TECHNOLOGY_FAMILY },
  { kTypeProperty, PROPERTY_INDEX_TYPE },
  { kUsageUrlProperty, PROPERTY_INDEX_USAGE_URL },
  { kWifiAuthMode, PROPERTY_INDEX_WIFI_AUTH_MODE },
  { kWifiFrequency, PROPERTY_INDEX_WIFI_FREQUENCY },
  { kWifiHexSsid, PROPERTY_INDEX_WIFI_HEX_SSID },
  { kWifiHiddenSsid, PROPERTY_INDEX_WIFI_HIDDEN_SSID },
  { kWifiPhyMode, PROPERTY_INDEX_WIFI_PHY_MODE },
};

// Serve the singleton mapper instance.
const EnumMapper<PropertyIndex>* get_native_mapper() {
  static const EnumMapper<PropertyIndex> mapper(property_index_table,
                                                arraysize(property_index_table),
                                                PROPERTY_INDEX_UNKNOWN);
  return &mapper;
}

ConnectionType ParseNetworkType(const std::string& type) {
  static EnumMapper<ConnectionType>::Pair table[] = {
    { kTypeEthernet, TYPE_ETHERNET },
    { kTypeWifi, TYPE_WIFI },
    { kTypeWimax, TYPE_WIMAX },
    { kTypeBluetooth, TYPE_BLUETOOTH },
    { kTypeCellular, TYPE_CELLULAR },
    { kTypeVpn, TYPE_VPN },
  };
  static EnumMapper<ConnectionType> parser(
      table, arraysize(table), TYPE_UNKNOWN);
  return parser.Get(type);
}

}  // namespace

// -------------------- NativeNetworkDeviceParser --------------------

NativeNetworkDeviceParser::NativeNetworkDeviceParser()
    : NetworkDeviceParser(get_native_mapper()) {
}

NativeNetworkDeviceParser::~NativeNetworkDeviceParser() {
}

bool NativeNetworkDeviceParser::ParseValue(PropertyIndex index,
                                           Value* value,
                                           NetworkDevice* device) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        device->set_type(ParseType(type_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value->GetAsString(&name))
        return false;
      device->set_name(name);
      return true;
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value->GetAsString(&unique_id))
        return false;
      device->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_CARRIER: {
      std::string carrier;
      if (!value->GetAsString(&carrier))
        return false;
      device->set_carrier(carrier);
      return true;
    }
    case PROPERTY_INDEX_SCANNING: {
      bool scanning;
      if (!value->GetAsBoolean(&scanning))
        return false;
      device->set_scanning(scanning);
      return true;
    }
    case PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING: {
      bool data_roaming_allowed;
      if (!value->GetAsBoolean(&data_roaming_allowed))
        return false;
      device->set_data_roaming_allowed(data_roaming_allowed);
      return true;
    }
    case PROPERTY_INDEX_CELLULAR_APN_LIST:
      if (ListValue* list = value->AsList()) {
        CellularApnList provider_apn_list;
        if (!ParseApnList(*list, &provider_apn_list))
          return false;
        device->set_provider_apn_list(provider_apn_list);
        return true;
      }
      break;
    case PROPERTY_INDEX_NETWORKS:
      if (value->AsList()) {
        // Ignored.
        return true;
      }
      break;
    case PROPERTY_INDEX_FOUND_NETWORKS:
      if (ListValue* list = value->AsList()) {
        CellularNetworkList found_cellular_networks;
        if (!ParseFoundNetworksFromList(*list, &found_cellular_networks))
          return false;
        device->set_found_cellular_networks(found_cellular_networks);
        return true;
      }
      break;
    case PROPERTY_INDEX_HOME_PROVIDER: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        DictionaryValue* dict = static_cast<DictionaryValue*>(value);
        std::string home_provider_code;
        std::string home_provider_country;
        std::string home_provider_name;
        dict->GetStringWithoutPathExpansion(kOperatorCodeKey,
                                            &home_provider_code);
        dict->GetStringWithoutPathExpansion(kOperatorCountryKey,
                                            &home_provider_country);
        dict->GetStringWithoutPathExpansion(kOperatorNameKey,
                                            &home_provider_name);
        device->set_home_provider_code(home_provider_code);
        device->set_home_provider_country(home_provider_country);
        device->set_home_provider_name(home_provider_name);
        if (!device->home_provider_name().empty() &&
            !device->home_provider_country().empty()) {
          device->set_home_provider_id(base::StringPrintf(
              kCarrierIdFormat,
              device->home_provider_name().c_str(),
              device->home_provider_country().c_str()));
        } else {
          device->set_home_provider_id(home_provider_code);
          LOG(WARNING) << "Carrier ID not defined, using code instead: "
                       << device->home_provider_id();
        }
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MEID:
    case PROPERTY_INDEX_IMEI:
    case PROPERTY_INDEX_IMSI:
    case PROPERTY_INDEX_ESN:
    case PROPERTY_INDEX_MDN:
    case PROPERTY_INDEX_MIN:
    case PROPERTY_INDEX_MODEL_ID:
    case PROPERTY_INDEX_MANUFACTURER:
    case PROPERTY_INDEX_FIRMWARE_REVISION:
    case PROPERTY_INDEX_HARDWARE_REVISION:
    case PROPERTY_INDEX_SELECTED_NETWORK: {
      std::string item;
      if (!value->GetAsString(&item))
        return false;
      switch (index) {
        case PROPERTY_INDEX_MEID:
          device->set_meid(item);
          break;
        case PROPERTY_INDEX_IMEI:
          device->set_imei(item);
          break;
        case PROPERTY_INDEX_IMSI:
          device->set_imsi(item);
          break;
        case PROPERTY_INDEX_ESN:
          device->set_esn(item);
          break;
        case PROPERTY_INDEX_MDN:
          device->set_mdn(item);
          break;
        case PROPERTY_INDEX_MIN:
          device->set_min(item);
          break;
        case PROPERTY_INDEX_MODEL_ID:
          device->set_model_id(item);
          break;
        case PROPERTY_INDEX_MANUFACTURER:
          device->set_manufacturer(item);
          break;
        case PROPERTY_INDEX_FIRMWARE_REVISION:
          device->set_firmware_revision(item);
          break;
        case PROPERTY_INDEX_HARDWARE_REVISION:
          device->set_hardware_revision(item);
          break;
        case PROPERTY_INDEX_SELECTED_NETWORK:
          device->set_selected_cellular_network(item);
          break;
        default:
          break;
      }
      return true;
    }
    case PROPERTY_INDEX_SIM_LOCK:
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        SimLockState sim_lock_state;
        int sim_retries_left;
        if (!ParseSimLockStateFromDictionary(
                static_cast<const DictionaryValue&>(*value),
                &sim_lock_state,
                &sim_retries_left))
          return false;
        device->set_sim_lock_state(sim_lock_state);
        device->set_sim_retries_left(sim_retries_left);
        // Initialize PinRequired value only once.
        // See SimPinRequire enum comments.
        if (device->sim_pin_required() == SIM_PIN_REQUIRE_UNKNOWN) {
          if (device->sim_lock_state() == SIM_UNLOCKED) {
            device->set_sim_pin_required(SIM_PIN_NOT_REQUIRED);
          } else if (device->sim_lock_state() == SIM_LOCKED_PIN ||
                     device->sim_lock_state() == SIM_LOCKED_PUK) {
            device->set_sim_pin_required(SIM_PIN_REQUIRED);
          }
        }
        return true;
      }
      break;
    case PROPERTY_INDEX_POWERED:
      // we don't care about the value, just the fact that it changed
      return true;
    case PROPERTY_INDEX_PRL_VERSION: {
      int prl_version;
      if (!value->GetAsInteger(&prl_version))
        return false;
      device->set_prl_version(prl_version);
      return true;
    }
    case PROPERTY_INDEX_SUPPORT_NETWORK_SCAN: {
      bool support_network_scan;
      if (!value->GetAsBoolean(&support_network_scan))
        return false;
      device->set_support_network_scan(support_network_scan);
      return true;
    }
    case PROPERTY_INDEX_TECHNOLOGY_FAMILY: {
      std::string technology_family_string;
      if (value->GetAsString(&technology_family_string)) {
        device->set_technology_family(
            ParseTechnologyFamily(technology_family_string));
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

ConnectionType NativeNetworkDeviceParser::ParseType(const std::string& type) {
  return ParseNetworkType(type);
}

bool NativeNetworkDeviceParser::ParseApnList(const ListValue& list,
                                             CellularApnList* apn_list) {
  apn_list->clear();
  apn_list->reserve(list.GetSize());
  for (ListValue::const_iterator it = list.begin(); it != list.end(); ++it) {
    if ((*it)->IsType(Value::TYPE_DICTIONARY)) {
      apn_list->resize(apn_list->size() + 1);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(*it);
      dict->GetStringWithoutPathExpansion(
          kApnProperty, &apn_list->back().apn);
      dict->GetStringWithoutPathExpansion(
          kApnNetworkIdProperty, &apn_list->back().network_id);
      dict->GetStringWithoutPathExpansion(
          kApnUsernameProperty, &apn_list->back().username);
      dict->GetStringWithoutPathExpansion(
          kApnPasswordProperty, &apn_list->back().password);
      dict->GetStringWithoutPathExpansion(
          kApnNameProperty, &apn_list->back().name);
      dict->GetStringWithoutPathExpansion(
          kApnLocalizedNameProperty, &apn_list->back().localized_name);
      dict->GetStringWithoutPathExpansion(
          kApnLanguageProperty, &apn_list->back().language);
    } else {
      return false;
    }
  }
  return true;
}

bool NativeNetworkDeviceParser::ParseFoundNetworksFromList(
    const ListValue& list,
    CellularNetworkList* found_networks) {
  found_networks->clear();
  found_networks->reserve(list.GetSize());
  for (ListValue::const_iterator it = list.begin(); it != list.end(); ++it) {
    if ((*it)->IsType(Value::TYPE_DICTIONARY)) {
      found_networks->resize(found_networks->size() + 1);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(*it);
      dict->GetStringWithoutPathExpansion(
          kStatusProperty, &found_networks->back().status);
      dict->GetStringWithoutPathExpansion(
          kNetworkIdProperty, &found_networks->back().network_id);
      dict->GetStringWithoutPathExpansion(
          kShortNameProperty, &found_networks->back().short_name);
      dict->GetStringWithoutPathExpansion(
          kLongNameProperty, &found_networks->back().long_name);
      dict->GetStringWithoutPathExpansion(
          kTechnologyProperty, &found_networks->back().technology);
    } else {
      return false;
    }
  }
  return true;
}

SimLockState NativeNetworkDeviceParser::ParseSimLockState(
    const std::string& state) {
  static EnumMapper<SimLockState>::Pair table[] = {
    { "", SIM_UNLOCKED },
    { kSimLockPin, SIM_LOCKED_PIN },
    { kSimLockPuk, SIM_LOCKED_PUK },
  };
  static EnumMapper<SimLockState> parser(
      table, arraysize(table), SIM_UNKNOWN);
  SimLockState parsed_state = parser.Get(state);
  DCHECK(parsed_state != SIM_UNKNOWN) << "Unknown SIMLock state encountered";
  return parsed_state;
}

bool NativeNetworkDeviceParser::ParseSimLockStateFromDictionary(
    const DictionaryValue& info, SimLockState* out_state, int* out_retries) {
  std::string state_string;
  if (!info.GetString(kSimLockTypeProperty, &state_string) ||
      !info.GetInteger(kSimLockRetriesLeftProperty, out_retries)) {
    LOG(ERROR) << "Error parsing SIMLock state";
    return false;
  }
  *out_state = ParseSimLockState(state_string);
  return true;
}

TechnologyFamily NativeNetworkDeviceParser::ParseTechnologyFamily(
    const std::string& technology_family) {
  static EnumMapper<TechnologyFamily>::Pair table[] = {
    { kTechnologyFamilyCdma, TECHNOLOGY_FAMILY_CDMA },
    { kTechnologyFamilyGsm, TECHNOLOGY_FAMILY_GSM },
  };
  static EnumMapper<TechnologyFamily> parser(
      table, arraysize(table), TECHNOLOGY_FAMILY_UNKNOWN);
  return parser.Get(technology_family);
}

// -------------------- NativeNetworkParser --------------------

NativeNetworkParser::NativeNetworkParser()
    : NetworkParser(get_native_mapper()) {
}

NativeNetworkParser::~NativeNetworkParser() {
}

// static
const EnumMapper<PropertyIndex>* NativeNetworkParser::property_mapper() {
  return get_native_mapper();

}

const ConnectionType NativeNetworkParser::ParseConnectionType(
    const std::string& connection_type) {
  return ParseNetworkType(connection_type);
}

bool NativeNetworkParser::ParseValue(PropertyIndex index,
                                     Value* value,
                                     Network* network) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        ConnectionType type = ParseType(type_string);
        LOG_IF(ERROR, type != network->type())
          << "Network with mismatched type: " << network->service_path()
          << " " << type << " != " << network->type();
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_DEVICE: {
      std::string device_path;
      if (!value->GetAsString(&device_path))
        return false;
      network->set_device_path(device_path);
      return true;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value->GetAsString(&name))
        return false;
      network->SetName(name);
      return true;
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value->GetAsString(&unique_id))
        return false;
      network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_PROFILE: {
      // Note: currently this is only provided for non remembered networks.
      std::string profile_path;
      if (!value->GetAsString(&profile_path))
        return false;
      network->set_profile_path(profile_path);
      return true;
    }
    case PROPERTY_INDEX_STATE: {
      std::string state_string;
      if (value->GetAsString(&state_string)) {
        network->SetState(ParseState(state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MODE: {
      std::string mode_string;
      if (value->GetAsString(&mode_string)) {
        network->set_mode(ParseMode(mode_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ERROR: {
      std::string error_string;
      if (value->GetAsString(&error_string)) {
        network->set_error(ParseError(error_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CONNECTABLE: {
      bool connectable;
      if (!value->GetAsBoolean(&connectable))
        return false;
      network->set_connectable(connectable);
      return true;
    }
    case PROPERTY_INDEX_IS_ACTIVE: {
      bool is_active;
      if (!value->GetAsBoolean(&is_active))
        return false;
      network->set_is_active(is_active);
      return true;
    }
    case PROPERTY_INDEX_FAVORITE:
      // This property is ignored.
      return true;
    case PROPERTY_INDEX_AUTO_CONNECT: {
      bool auto_connect;
      if (!value->GetAsBoolean(&auto_connect))
        return false;
      network->set_auto_connect(auto_connect);
      return true;
    }
    case PROPERTY_INDEX_SAVE_CREDENTIALS: {
      bool save_credentials;
      if (!value->GetAsBoolean(&save_credentials))
        return false;
      network->set_save_credentials(save_credentials);
      return true;
    }
    case PROPERTY_INDEX_PROXY_CONFIG: {
      std::string proxy_config;
      if (!value->GetAsString(&proxy_config))
        return false;
      network->set_proxy_config(proxy_config);
      return true;
    }
    default:
      break;
  }
  return false;
}

ConnectionType NativeNetworkParser::ParseType(const std::string& type) {
  return ParseNetworkType(type);
}

ConnectionType NativeNetworkParser::ParseTypeFromDictionary(
    const DictionaryValue& info) {
  std::string type_string;
  info.GetString(kTypeProperty, &type_string);
  return ParseType(type_string);
}

ConnectionMode NativeNetworkParser::ParseMode(const std::string& mode) {
  static EnumMapper<ConnectionMode>::Pair table[] = {
    { kModeManaged, MODE_MANAGED },
    { kModeAdhoc, MODE_ADHOC },
  };
  static EnumMapper<ConnectionMode> parser(
      table, arraysize(table), MODE_UNKNOWN);
  return parser.Get(mode);
}

ConnectionState NativeNetworkParser::ParseState(const std::string& state) {
  static EnumMapper<ConnectionState>::Pair table[] = {
    { kStateIdle, STATE_IDLE },
    { kStateCarrier, STATE_CARRIER },
    { kStateAssociation, STATE_ASSOCIATION },
    { kStateConfiguration, STATE_CONFIGURATION },
    { kStateReady, STATE_READY },
    { kStateDisconnect, STATE_DISCONNECT },
    { kStateFailure, STATE_FAILURE },
    { kStateActivationFailure, STATE_ACTIVATION_FAILURE },
    { kStatePortal, STATE_PORTAL },
    { kStateOnline, STATE_ONLINE },
  };
  static EnumMapper<ConnectionState> parser(
      table, arraysize(table), STATE_UNKNOWN);
  return parser.Get(state);
}

ConnectionError NativeNetworkParser::ParseError(const std::string& error) {
  static EnumMapper<ConnectionError>::Pair table[] = {
    { kErrorOutOfRange, ERROR_OUT_OF_RANGE },
    { kErrorPinMissing, ERROR_PIN_MISSING },
    { kErrorDhcpFailed, ERROR_DHCP_FAILED },
    { kErrorConnectFailed, ERROR_CONNECT_FAILED },
    { kErrorBadPassphrase, ERROR_BAD_PASSPHRASE },
    { kErrorBadWepKey, ERROR_BAD_WEPKEY },
    { kErrorActivationFailed, ERROR_ACTIVATION_FAILED },
    { kErrorNeedEvdo, ERROR_NEED_EVDO },
    { kErrorNeedHomeNetwork, ERROR_NEED_HOME_NETWORK },
    { kErrorOtaspFailed, ERROR_OTASP_FAILED },
    { kErrorAaaFailed, ERROR_AAA_FAILED },
    { kErrorInternal, ERROR_INTERNAL },
    { kErrorDnsLookupFailed, ERROR_DNS_LOOKUP_FAILED },
    { kErrorHttpGetFailed, ERROR_HTTP_GET_FAILED },
  };
  static EnumMapper<ConnectionError> parser(
      table, arraysize(table), ERROR_NO_ERROR);
  return parser.Get(error);
}

// -------------------- NativeEthernetNetworkParser --------------------

NativeEthernetNetworkParser::NativeEthernetNetworkParser() {}
NativeEthernetNetworkParser::~NativeEthernetNetworkParser() {}

// -------------------- NativeWirelessNetworkParser --------------------

NativeWirelessNetworkParser::NativeWirelessNetworkParser() {}
NativeWirelessNetworkParser::~NativeWirelessNetworkParser() {}

bool NativeWirelessNetworkParser::ParseValue(PropertyIndex index,
                                             Value* value,
                                             Network* network) {
  DCHECK_NE(TYPE_ETHERNET, network->type());
  DCHECK_NE(TYPE_VPN, network->type());
  WirelessNetwork* wireless_network = static_cast<WirelessNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_SIGNAL_STRENGTH: {
      int strength;
      if (!value->GetAsInteger(&strength))
        return false;
      wireless_network->set_strength(strength);
      return true;
    }
    default:
      return NativeNetworkParser::ParseValue(index, value, network);
      break;
  }
  return false;
}

// -------------------- NativeCellularNetworkParser --------------------

NativeCellularNetworkParser::NativeCellularNetworkParser() {}
NativeCellularNetworkParser::~NativeCellularNetworkParser() {}

bool NativeCellularNetworkParser::ParseValue(PropertyIndex index,
                                             Value* value,
                                             Network* network) {
  DCHECK_EQ(TYPE_CELLULAR, network->type());
  CellularNetwork* cellular_network = static_cast<CellularNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_ACTIVATION_STATE: {
      std::string activation_state_string;
      if (value->GetAsString(&activation_state_string)) {
        ActivationState prev_state = cellular_network->activation_state();
        cellular_network->set_activation_state(
            ParseActivationState(activation_state_string));
        if (cellular_network->activation_state() != prev_state)
          cellular_network->RefreshDataPlansIfNeeded();
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_APN: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        cellular_network->set_apn(static_cast<const DictionaryValue&>(*value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        cellular_network->set_last_good_apn(
            static_cast<const DictionaryValue&>(*value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NETWORK_TECHNOLOGY: {
      std::string network_technology_string;
      if (value->GetAsString(&network_technology_string)) {
        cellular_network->set_network_technology(
            ParseNetworkTechnology(network_technology_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ROAMING_STATE: {
      std::string roaming_state_string;
      if (value->GetAsString(&roaming_state_string)) {
        cellular_network->set_roaming_state(
            ParseRoamingState(roaming_state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_OPERATOR_NAME: {
      std::string value_str;
      if (!value->GetAsString(&value_str))
        break;
      cellular_network->set_operator_name(value_str);
      return true;
    }
    case PROPERTY_INDEX_OPERATOR_CODE: {
      std::string value_str;
      if (!value->GetAsString(&value_str))
        break;
      cellular_network->set_operator_code(value_str);
      return true;
    }
    case PROPERTY_INDEX_SERVING_OPERATOR: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        const DictionaryValue& dict =
            static_cast<const DictionaryValue&>(*value);
        std::string value_str;
        dict.GetStringWithoutPathExpansion(kOperatorNameKey, &value_str);
        cellular_network->set_operator_name(value_str);
        value_str.clear();
        dict.GetStringWithoutPathExpansion(kOperatorCodeKey, &value_str);
        cellular_network->set_operator_code(value_str);
        value_str.clear();
        dict.GetStringWithoutPathExpansion(kOperatorCountryKey, &value_str);
        cellular_network->set_operator_country(value_str);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PAYMENT_URL: {
      std::string value_str;
      if (!value->GetAsString(&value_str))
        break;
      cellular_network->set_payment_url(value_str);
      return true;
    }
    case PROPERTY_INDEX_USAGE_URL: {
      std::string value_str;
      if (!value->GetAsString(&value_str))
        break;
      cellular_network->set_usage_url(value_str);
      return true;
    }
    case PROPERTY_INDEX_STATE: {
      // Save previous state before calling WirelessNetwork::ParseValue.
      ConnectionState prev_state = cellular_network->state();
      if (NativeWirelessNetworkParser::ParseValue(index, value, network)) {
        if (cellular_network->state() != prev_state)
          cellular_network->RefreshDataPlansIfNeeded();
        return true;
      }
      break;
    }
    default:
      return NativeWirelessNetworkParser::ParseValue(index, value, network);
  }
  return false;
}

ActivationState NativeCellularNetworkParser::ParseActivationState(
    const std::string& state) {
  static EnumMapper<ActivationState>::Pair table[] = {
    { kActivationStateActivated, ACTIVATION_STATE_ACTIVATED },
    { kActivationStateActivating, ACTIVATION_STATE_ACTIVATING },
    { kActivationStateNotActivated, ACTIVATION_STATE_NOT_ACTIVATED },
    { kActivationStatePartiallyActivated, ACTIVATION_STATE_PARTIALLY_ACTIVATED},
    { kActivationStateUnknown, ACTIVATION_STATE_UNKNOWN},
  };
  static EnumMapper<ActivationState> parser(
      table, arraysize(table), ACTIVATION_STATE_UNKNOWN);
  return parser.Get(state);
}

NetworkTechnology NativeCellularNetworkParser::ParseNetworkTechnology(
    const std::string& technology) {
  static EnumMapper<NetworkTechnology>::Pair table[] = {
    { kNetworkTechnology1Xrtt, NETWORK_TECHNOLOGY_1XRTT },
    { kNetworkTechnologyEvdo, NETWORK_TECHNOLOGY_EVDO },
    { kNetworkTechnologyGprs, NETWORK_TECHNOLOGY_GPRS },
    { kNetworkTechnologyEdge, NETWORK_TECHNOLOGY_EDGE },
    { kNetworkTechnologyUmts, NETWORK_TECHNOLOGY_UMTS },
    { kNetworkTechnologyHspa, NETWORK_TECHNOLOGY_HSPA },
    { kNetworkTechnologyHspaPlus, NETWORK_TECHNOLOGY_HSPA_PLUS },
    { kNetworkTechnologyLte, NETWORK_TECHNOLOGY_LTE },
    { kNetworkTechnologyLteAdvanced, NETWORK_TECHNOLOGY_LTE_ADVANCED },
    { kNetworkTechnologyGsm, NETWORK_TECHNOLOGY_GSM },
  };
  static EnumMapper<NetworkTechnology> parser(
      table, arraysize(table), NETWORK_TECHNOLOGY_UNKNOWN);
  return parser.Get(technology);
}

NetworkRoamingState NativeCellularNetworkParser::ParseRoamingState(
    const std::string& roaming_state) {
  static EnumMapper<NetworkRoamingState>::Pair table[] = {
    { kRoamingStateHome, ROAMING_STATE_HOME },
    { kRoamingStateRoaming, ROAMING_STATE_ROAMING },
    { kRoamingStateUnknown, ROAMING_STATE_UNKNOWN },
  };
  static EnumMapper<NetworkRoamingState> parser(
      table, arraysize(table), ROAMING_STATE_UNKNOWN);
  return parser.Get(roaming_state);
}

// -------------------- NativeWifiNetworkParser --------------------

NativeWifiNetworkParser::NativeWifiNetworkParser() {}
NativeWifiNetworkParser::~NativeWifiNetworkParser() {}

bool NativeWifiNetworkParser::ParseValue(PropertyIndex index,
                                         Value* value,
                                         Network* network) {
  DCHECK_EQ(TYPE_WIFI, network->type());
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_WIFI_HEX_SSID: {
      std::string ssid_hex;
      if (!value->GetAsString(&ssid_hex))
        return false;

      wifi_network->SetHexSsid(ssid_hex);
      return true;
    }
    case PROPERTY_INDEX_WIFI_AUTH_MODE:
    case PROPERTY_INDEX_WIFI_PHY_MODE:
    case PROPERTY_INDEX_WIFI_HIDDEN_SSID:
    case PROPERTY_INDEX_WIFI_FREQUENCY:
      // These properties are currently not used in the UI.
      return true;
    case PROPERTY_INDEX_NAME: {
      // Does not change network name when it was already set by WiFi.HexSSID.
      if (!wifi_network->name().empty())
        return true;
      else
        return NativeWirelessNetworkParser::ParseValue(index, value, network);
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value->GetAsString(&unique_id))
        break;
      wifi_network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_SECURITY: {
      std::string security_string;
      if (!value->GetAsString(&security_string))
        break;
      wifi_network->set_encryption(ParseSecurity(security_string));
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE: {
      std::string passphrase;
      if (!value->GetAsString(&passphrase))
        break;
      // Only store the passphrase if we are the owner.
      // TODO(stevenjb): Remove this when chromium-os:12948 is resolved.
      if (chromeos::UserManager::Get()->current_user_is_owner())
        wifi_network->set_passphrase(passphrase);
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED: {
      bool passphrase_required;
      value->GetAsBoolean(&passphrase_required);
      if (!value->GetAsBoolean(&passphrase_required))
        break;
      wifi_network->set_passphrase_required(passphrase_required);
      return true;
    }
    case PROPERTY_INDEX_IDENTITY: {
      std::string identity;
      if (!value->GetAsString(&identity))
        break;
      wifi_network->set_identity(identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_IDENTITY: {
      std::string eap_identity;
      if (!value->GetAsString(&eap_identity))
        break;
      wifi_network->set_eap_identity(eap_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_METHOD: {
      std::string eap_method;
      if (!value->GetAsString(&eap_method))
        break;
      wifi_network->set_eap_method(ParseEAPMethod(eap_method));
      return true;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      std::string eap_phase_2_auth;
      if (!value->GetAsString(&eap_phase_2_auth))
        break;
      wifi_network->set_eap_phase_2_auth(ParseEAPPhase2Auth(eap_phase_2_auth));
      return true;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY: {
      std::string eap_anonymous_identity;
      if (!value->GetAsString(&eap_anonymous_identity))
        break;
      wifi_network->set_eap_anonymous_identity(eap_anonymous_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_CERT_ID: {
      std::string eap_client_cert_pkcs11_id;
      if (!value->GetAsString(&eap_client_cert_pkcs11_id))
        break;
      wifi_network->set_eap_client_cert_pkcs11_id(eap_client_cert_pkcs11_id);
      return true;
    }
    case PROPERTY_INDEX_EAP_CA_CERT_NSS: {
      std::string eap_server_ca_cert_nss_nickname;
      if (!value->GetAsString(&eap_server_ca_cert_nss_nickname))
        break;
      wifi_network->set_eap_server_ca_cert_nss_nickname(
          eap_server_ca_cert_nss_nickname);
      return true;
    }
    case PROPERTY_INDEX_EAP_USE_SYSTEM_CAS: {
      bool eap_use_system_cas;
      if (!value->GetAsBoolean(&eap_use_system_cas))
        break;
      wifi_network->set_eap_use_system_cas(eap_use_system_cas);
      return true;
    }
    case PROPERTY_INDEX_EAP_PASSWORD: {
      std::string eap_passphrase;
      if (!value->GetAsString(&eap_passphrase))
        break;
      wifi_network->set_eap_passphrase(eap_passphrase);
      return true;
    }
    case PROPERTY_INDEX_EAP_CLIENT_CERT:
    case PROPERTY_INDEX_EAP_CLIENT_CERT_NSS:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD:
    case PROPERTY_INDEX_EAP_KEY_ID:
    case PROPERTY_INDEX_EAP_CA_CERT:
    case PROPERTY_INDEX_EAP_CA_CERT_ID:
    case PROPERTY_INDEX_EAP_PIN:
    case PROPERTY_INDEX_EAP_KEY_MGMT:
      // These properties are currently not used in the UI.
      return true;
    default:
      return NativeWirelessNetworkParser::ParseValue(index, value, network);
  }
  return false;
}

ConnectionSecurity NativeWifiNetworkParser::ParseSecurity(
    const std::string& security) {
  static EnumMapper<ConnectionSecurity>::Pair table[] = {
    { kSecurityNone, SECURITY_NONE },
    { kSecurityWep, SECURITY_WEP },
    { kSecurityWpa, SECURITY_WPA },
    { kSecurityRsn, SECURITY_RSN },
    { kSecurityPsk, SECURITY_PSK },
    { kSecurity8021x, SECURITY_8021X },
  };
  static EnumMapper<ConnectionSecurity> parser(
      table, arraysize(table), SECURITY_UNKNOWN);
  return parser.Get(security);
}

EAPMethod NativeWifiNetworkParser::ParseEAPMethod(const std::string& method) {
  static EnumMapper<EAPMethod>::Pair table[] = {
    { kEapMethodPeap, EAP_METHOD_PEAP },
    { kEapMethodTls, EAP_METHOD_TLS },
    { kEapMethodTtls, EAP_METHOD_TTLS },
    { kEapMethodLeap, EAP_METHOD_LEAP },
  };
  static EnumMapper<EAPMethod> parser(
      table, arraysize(table), EAP_METHOD_UNKNOWN);
  return parser.Get(method);
}

EAPPhase2Auth NativeWifiNetworkParser::ParseEAPPhase2Auth(
    const std::string& auth) {
  static EnumMapper<EAPPhase2Auth>::Pair table[] = {
    { kEapPhase2AuthPeapMd5, EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthPeapMschap2, EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTtlsMd5, EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthTtlsMschapV2, EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTtlsMschap, EAP_PHASE_2_AUTH_MSCHAP },
    { kEapPhase2AuthTtlsPap, EAP_PHASE_2_AUTH_PAP },
    { kEapPhase2AuthTtlsChap, EAP_PHASE_2_AUTH_CHAP },
  };
  static EnumMapper<EAPPhase2Auth> parser(
      table, arraysize(table), EAP_PHASE_2_AUTH_AUTO);
  return parser.Get(auth);
}

// -------------------- NativeVirtualNetworkParser --------------------


NativeVirtualNetworkParser::NativeVirtualNetworkParser() {}
NativeVirtualNetworkParser::~NativeVirtualNetworkParser() {}

bool NativeVirtualNetworkParser::UpdateNetworkFromInfo(
    const DictionaryValue& info,
    Network* network) {
  DCHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  if (!NativeNetworkParser::UpdateNetworkFromInfo(info, network))
    return false;
  VLOG(1) << "Updating VPN '" << virtual_network->name()
          << "': Server: " << virtual_network->server_hostname()
          << " Type: "
          << ProviderTypeToString(virtual_network->provider_type());
  if (virtual_network->provider_type() == PROVIDER_TYPE_L2TP_IPSEC_PSK) {
    if (!virtual_network->client_cert_id().empty())
      virtual_network->set_provider_type(PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  }
  return true;
}

bool NativeVirtualNetworkParser::ParseValue(PropertyIndex index,
                                            Value* value,
                                            Network* network) {
  DCHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PROVIDER: {
      DCHECK_EQ(value->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue& dict = static_cast<const DictionaryValue&>(*value);
      for (DictionaryValue::key_iterator iter = dict.begin_keys();
           iter != dict.end_keys(); ++iter) {
        const std::string& key = *iter;
        Value* provider_value;
        bool res = dict.GetWithoutPathExpansion(key, &provider_value);
        DCHECK(res);
        if (res) {
          PropertyIndex index = mapper().Get(key);
          if (!ParseProviderValue(index, *provider_value, virtual_network))
            VLOG(1) << network->name() << ": Provider unhandled key: " << key
                    << " Type: " << provider_value->GetType();
        }
      }
      return true;
    }
    default:
      return NativeNetworkParser::ParseValue(index, value, network);
      break;
  }
  return false;
}

bool NativeVirtualNetworkParser::ParseProviderValue(PropertyIndex index,
                                                    const Value& value,
                                                    VirtualNetwork* network) {
  switch (index) {
    case PROPERTY_INDEX_HOST: {
      std::string server_hostname;
      if (!value.GetAsString(&server_hostname))
        break;
      network->set_server_hostname(server_hostname);
      return true;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value.GetAsString(&name))
        break;
      network->set_name(name);
      return true;
    }
    case PROPERTY_INDEX_TYPE: {
      std::string provider_type_string;
      if (!value.GetAsString(&provider_type_string))
        break;
      network->set_provider_type(ParseProviderType(provider_type_string));
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS: {
      std::string ca_cert_nss;
      if (!value.GetAsString(&ca_cert_nss))
        break;
      network->set_ca_cert_nss(ca_cert_nss);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PSK:{
      std::string psk_passphrase;
      if (!value.GetAsString(&psk_passphrase))
        break;
      network->set_psk_passphrase(psk_passphrase);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID:{
      std::string client_cert_id;
      if (!value.GetAsString(&client_cert_id))
        break;
      network->set_client_cert_id(client_cert_id);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_USER:{
      std::string username;
      if (!value.GetAsString(&username))
        break;
      network->set_username(username);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PASSWORD:{
      std::string user_passphrase;
      if (!value.GetAsString(&user_passphrase))
        break;
      network->set_user_passphrase(user_passphrase);
      return true;
    }
    default:
      break;
  }
  return false;
}

ProviderType NativeVirtualNetworkParser::ParseProviderType(
    const std::string& type) {
  static EnumMapper<ProviderType>::Pair table[] = {
    { kProviderL2tpIpsec, PROVIDER_TYPE_L2TP_IPSEC_PSK },
    { kProviderOpenVpn, PROVIDER_TYPE_OPEN_VPN },
  };
  static EnumMapper<ProviderType> parser(
      table, arraysize(table), PROVIDER_TYPE_MAX);
  return parser.Get(type);
}

}  // namespace chromeos
