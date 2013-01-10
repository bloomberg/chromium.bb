// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/native_network_parser.h"

#include <string>

#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/native_network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

// Local constants.
namespace {

const char kPostMethod[] = "post";

EnumMapper<PropertyIndex>::Pair property_index_table[] = {
  { shill::kActivateOverNonCellularNetworkProperty,
    PROPERTY_INDEX_ACTIVATE_OVER_NON_CELLULAR_NETWORK },
  { flimflam::kActivationStateProperty, PROPERTY_INDEX_ACTIVATION_STATE },
  { flimflam::kActiveProfileProperty, PROPERTY_INDEX_ACTIVE_PROFILE },
  { flimflam::kArpGatewayProperty, PROPERTY_INDEX_ARP_GATEWAY },
  { flimflam::kAutoConnectProperty, PROPERTY_INDEX_AUTO_CONNECT },
  { flimflam::kAvailableTechnologiesProperty,
    PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES },
  { flimflam::kCarrierProperty, PROPERTY_INDEX_CARRIER },
  { flimflam::kCellularAllowRoamingProperty,
    PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING },
  { flimflam::kCellularApnListProperty, PROPERTY_INDEX_CELLULAR_APN_LIST },
  { flimflam::kCellularApnProperty, PROPERTY_INDEX_CELLULAR_APN },
  { flimflam::kCellularLastGoodApnProperty,
    PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN },
  { flimflam::kCheckPortalProperty, PROPERTY_INDEX_CHECK_PORTAL },
  { flimflam::kCheckPortalListProperty, PROPERTY_INDEX_CHECK_PORTAL_LIST },
  { flimflam::kConnectableProperty, PROPERTY_INDEX_CONNECTABLE },
  { flimflam::kConnectedTechnologiesProperty,
    PROPERTY_INDEX_CONNECTED_TECHNOLOGIES },
  { flimflam::kDefaultTechnologyProperty, PROPERTY_INDEX_DEFAULT_TECHNOLOGY },
  { flimflam::kDeviceProperty, PROPERTY_INDEX_DEVICE },
  { flimflam::kDevicesProperty, PROPERTY_INDEX_DEVICES },
  { flimflam::kEapAnonymousIdentityProperty,
    PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY },
  { flimflam::kEapCaCertIdProperty, PROPERTY_INDEX_EAP_CA_CERT_ID },
  { flimflam::kEapCaCertNssProperty, PROPERTY_INDEX_EAP_CA_CERT_NSS },
  { flimflam::kEapCaCertProperty, PROPERTY_INDEX_EAP_CA_CERT },
  { flimflam::kEapCertIdProperty, PROPERTY_INDEX_EAP_CERT_ID },
  { flimflam::kEapClientCertNssProperty, PROPERTY_INDEX_EAP_CLIENT_CERT_NSS },
  { flimflam::kEapClientCertProperty, PROPERTY_INDEX_EAP_CLIENT_CERT },
  { flimflam::kEapIdentityProperty, PROPERTY_INDEX_EAP_IDENTITY },
  { flimflam::kEapKeyIdProperty, PROPERTY_INDEX_EAP_KEY_ID },
  { flimflam::kEapKeyMgmtProperty, PROPERTY_INDEX_EAP_KEY_MGMT },
  { flimflam::kEapMethodProperty, PROPERTY_INDEX_EAP_METHOD },
  { flimflam::kEapPasswordProperty, PROPERTY_INDEX_EAP_PASSWORD },
  { flimflam::kEapPhase2AuthProperty, PROPERTY_INDEX_EAP_PHASE_2_AUTH },
  { flimflam::kEapPinProperty, PROPERTY_INDEX_EAP_PIN },
  { flimflam::kEapPrivateKeyPasswordProperty,
    PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD },
  { flimflam::kEapPrivateKeyProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY },
  { flimflam::kEapUseSystemCasProperty, PROPERTY_INDEX_EAP_USE_SYSTEM_CAS },
  { flimflam::kEnabledTechnologiesProperty,
    PROPERTY_INDEX_ENABLED_TECHNOLOGIES },
  { flimflam::kErrorProperty, PROPERTY_INDEX_ERROR },
  { flimflam::kEsnProperty, PROPERTY_INDEX_ESN },
  { flimflam::kFavoriteProperty, PROPERTY_INDEX_FAVORITE },
  { flimflam::kFirmwareRevisionProperty, PROPERTY_INDEX_FIRMWARE_REVISION },
  { flimflam::kFoundNetworksProperty, PROPERTY_INDEX_FOUND_NETWORKS },
  { flimflam::kGuidProperty, PROPERTY_INDEX_GUID },
  { flimflam::kHardwareRevisionProperty, PROPERTY_INDEX_HARDWARE_REVISION },
  { flimflam::kHomeProviderProperty, PROPERTY_INDEX_HOME_PROVIDER },
  { flimflam::kHostProperty, PROPERTY_INDEX_HOST },
  { flimflam::kIccidProperty, PROPERTY_INDEX_ICCID },
  { flimflam::kIdentityProperty, PROPERTY_INDEX_IDENTITY },
  { flimflam::kImeiProperty, PROPERTY_INDEX_IMEI },
  { flimflam::kImsiProperty, PROPERTY_INDEX_IMSI },
  { flimflam::kIsActiveProperty, PROPERTY_INDEX_IS_ACTIVE },
  { flimflam::kL2tpIpsecAuthenticationType,
    PROPERTY_INDEX_IPSEC_AUTHENTICATIONTYPE },
  { flimflam::kL2tpIpsecCaCertNssProperty,
    PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS },
  { flimflam::kL2tpIpsecClientCertIdProperty,
    PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID },
  { flimflam::kL2tpIpsecClientCertSlotProp,
    PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT },
  { flimflam::kL2tpIpsecIkeVersion, PROPERTY_INDEX_IPSEC_IKEVERSION },
  { flimflam::kL2tpIpsecPinProperty, PROPERTY_INDEX_L2TPIPSEC_PIN },
  { flimflam::kL2tpIpsecPskProperty, PROPERTY_INDEX_L2TPIPSEC_PSK },
  { flimflam::kL2tpIpsecPskRequiredProperty,
    PROPERTY_INDEX_L2TPIPSEC_PSK_REQUIRED },
  { flimflam::kL2tpIpsecPasswordProperty, PROPERTY_INDEX_L2TPIPSEC_PASSWORD },
  { flimflam::kL2tpIpsecUserProperty, PROPERTY_INDEX_L2TPIPSEC_USER },
  { flimflam::kL2tpIpsecGroupNameProperty,
    PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME },
  { flimflam::kManufacturerProperty, PROPERTY_INDEX_MANUFACTURER },
  { flimflam::kMdnProperty, PROPERTY_INDEX_MDN },
  { flimflam::kMeidProperty, PROPERTY_INDEX_MEID },
  { flimflam::kMinProperty, PROPERTY_INDEX_MIN },
  { flimflam::kModeProperty, PROPERTY_INDEX_MODE },
  { flimflam::kModelIDProperty, PROPERTY_INDEX_MODEL_ID },
  { flimflam::kNameProperty, PROPERTY_INDEX_NAME },
  { flimflam::kNetworkTechnologyProperty, PROPERTY_INDEX_NETWORK_TECHNOLOGY },
  { flimflam::kNetworksProperty, PROPERTY_INDEX_NETWORKS },
  { flimflam::kOfflineModeProperty, PROPERTY_INDEX_OFFLINE_MODE },
  { flimflam::kOperatorCodeProperty, PROPERTY_INDEX_OPERATOR_CODE },
  { flimflam::kOperatorNameProperty, PROPERTY_INDEX_OPERATOR_NAME },
  { flimflam::kPRLVersionProperty, PROPERTY_INDEX_PRL_VERSION },
  { flimflam::kPassphraseProperty, PROPERTY_INDEX_PASSPHRASE },
  { flimflam::kPassphraseRequiredProperty, PROPERTY_INDEX_PASSPHRASE_REQUIRED },
  { flimflam::kPortalURLProperty, PROPERTY_INDEX_PORTAL_URL },
  { flimflam::kPoweredProperty, PROPERTY_INDEX_POWERED },
  { flimflam::kPriorityProperty, PROPERTY_INDEX_PRIORITY },
  { flimflam::kProfileProperty, PROPERTY_INDEX_PROFILE },
  { flimflam::kProfilesProperty, PROPERTY_INDEX_PROFILES },
  { flimflam::kProviderHostProperty, PROPERTY_INDEX_PROVIDER_HOST },
  { flimflam::kProviderProperty, PROPERTY_INDEX_PROVIDER },
  { shill::kProviderRequiresRoamingProperty,
    PROPERTY_INDEX_PROVIDER_REQUIRES_ROAMING },
  { flimflam::kProviderTypeProperty, PROPERTY_INDEX_PROVIDER_TYPE },
  { flimflam::kProxyConfigProperty, PROPERTY_INDEX_PROXY_CONFIG },
  { flimflam::kRoamingStateProperty, PROPERTY_INDEX_ROAMING_STATE },
  { flimflam::kSIMLockStatusProperty, PROPERTY_INDEX_SIM_LOCK },
  { shill::kSIMPresentProperty, PROPERTY_INDEX_SIM_PRESENT },
  { flimflam::kSSIDProperty, PROPERTY_INDEX_SSID },
  { flimflam::kSaveCredentialsProperty, PROPERTY_INDEX_SAVE_CREDENTIALS },
  { flimflam::kScanningProperty, PROPERTY_INDEX_SCANNING },
  { flimflam::kSecurityProperty, PROPERTY_INDEX_SECURITY },
  { flimflam::kSelectedNetworkProperty, PROPERTY_INDEX_SELECTED_NETWORK },
  { flimflam::kServiceWatchListProperty, PROPERTY_INDEX_SERVICE_WATCH_LIST },
  { flimflam::kServicesProperty, PROPERTY_INDEX_SERVICES },
  { flimflam::kServingOperatorProperty, PROPERTY_INDEX_SERVING_OPERATOR },
  { flimflam::kSignalStrengthProperty, PROPERTY_INDEX_SIGNAL_STRENGTH },
  { flimflam::kStateProperty, PROPERTY_INDEX_STATE },
  { flimflam::kSupportNetworkScanProperty,
    PROPERTY_INDEX_SUPPORT_NETWORK_SCAN },
  { shill::kSupportedCarriersProperty, PROPERTY_INDEX_SUPPORTED_CARRIERS},
  { flimflam::kTechnologyFamilyProperty, PROPERTY_INDEX_TECHNOLOGY_FAMILY },
  { flimflam::kTypeProperty, PROPERTY_INDEX_TYPE },
  { flimflam::kUIDataProperty, PROPERTY_INDEX_UI_DATA },
  { flimflam::kUsageURLProperty, PROPERTY_INDEX_USAGE_URL },
  { flimflam::kOpenVPNClientCertIdProperty,
    PROPERTY_INDEX_OPEN_VPN_CLIENT_CERT_ID },
  { flimflam::kOpenVPNAuthProperty, PROPERTY_INDEX_OPEN_VPN_AUTH },
  { flimflam::kOpenVPNAuthRetryProperty, PROPERTY_INDEX_OPEN_VPN_AUTHRETRY },
  { flimflam::kOpenVPNAuthNoCacheProperty,
    PROPERTY_INDEX_OPEN_VPN_AUTHNOCACHE },
  { flimflam::kOpenVPNAuthUserPassProperty,
    PROPERTY_INDEX_OPEN_VPN_AUTHUSERPASS },
  { flimflam::kOpenVPNCaCertNSSProperty, PROPERTY_INDEX_OPEN_VPN_CACERT },
  { flimflam::kOpenVPNClientCertSlotProperty,
    PROPERTY_INDEX_OPEN_VPN_CLIENT_CERT_SLOT },
  { flimflam::kOpenVPNCipherProperty, PROPERTY_INDEX_OPEN_VPN_CIPHER },
  { flimflam::kOpenVPNCompLZOProperty, PROPERTY_INDEX_OPEN_VPN_COMPLZO },
  { flimflam::kOpenVPNCompNoAdaptProperty,
    PROPERTY_INDEX_OPEN_VPN_COMPNOADAPT },
  { flimflam::kOpenVPNKeyDirectionProperty,
    PROPERTY_INDEX_OPEN_VPN_KEYDIRECTION },
  { flimflam::kOpenVPNMgmtEnableProperty,
    PROPERTY_INDEX_OPEN_VPN_MGMT_ENABLE },
  { flimflam::kOpenVPNNsCertTypeProperty, PROPERTY_INDEX_OPEN_VPN_NSCERTTYPE },
  { flimflam::kOpenVPNOTPProperty, PROPERTY_INDEX_OPEN_VPN_OTP },
  { flimflam::kOpenVPNPasswordProperty, PROPERTY_INDEX_OPEN_VPN_PASSWORD },
  { flimflam::kOpenVPNPinProperty, PROPERTY_INDEX_OPEN_VPN_PIN },
  { flimflam::kOpenVPNPortProperty, PROPERTY_INDEX_OPEN_VPN_PORT },
  { flimflam::kOpenVPNProtoProperty, PROPERTY_INDEX_OPEN_VPN_PROTO },
  { flimflam::kOpenVPNProviderProperty,
    PROPERTY_INDEX_OPEN_VPN_PKCS11_PROVIDER },
  { flimflam::kOpenVPNPushPeerInfoProperty,
    PROPERTY_INDEX_OPEN_VPN_PUSHPEERINFO },
  { flimflam::kOpenVPNRemoteCertEKUProperty,
    PROPERTY_INDEX_OPEN_VPN_REMOTECERTEKU },
  { flimflam::kOpenVPNRemoteCertKUProperty,
    PROPERTY_INDEX_OPEN_VPN_REMOTECERTKU },
  { flimflam::kOpenVPNRemoteCertTLSProperty,
    PROPERTY_INDEX_OPEN_VPN_REMOTECERTTLS },
  { flimflam::kOpenVPNRenegSecProperty, PROPERTY_INDEX_OPEN_VPN_RENEGSEC },
  { flimflam::kOpenVPNServerPollTimeoutProperty,
    PROPERTY_INDEX_OPEN_VPN_SERVERPOLLTIMEOUT },
  { flimflam::kOpenVPNShaperProperty, PROPERTY_INDEX_OPEN_VPN_SHAPER },
  { flimflam::kOpenVPNStaticChallengeProperty,
    PROPERTY_INDEX_OPEN_VPN_STATICCHALLENGE },
  { flimflam::kOpenVPNTLSAuthContentsProperty,
    PROPERTY_INDEX_OPEN_VPN_TLSAUTHCONTENTS },
  { flimflam::kOpenVPNTLSRemoteProperty, PROPERTY_INDEX_OPEN_VPN_TLSREMOTE },
  { flimflam::kOpenVPNUserProperty, PROPERTY_INDEX_OPEN_VPN_USER },
  { flimflam::kPaymentPortalProperty, PROPERTY_INDEX_OLP },
  { flimflam::kPaymentURLProperty, PROPERTY_INDEX_OLP_URL },
  { flimflam::kVPNDomainProperty, PROPERTY_INDEX_VPN_DOMAIN },
  { flimflam::kWifiAuthMode, PROPERTY_INDEX_WIFI_AUTH_MODE },
  { flimflam::kWifiBSsid, PROPERTY_INDEX_WIFI_BSSID },
  { flimflam::kWifiFrequency, PROPERTY_INDEX_WIFI_FREQUENCY },
  { flimflam::kWifiHexSsid, PROPERTY_INDEX_WIFI_HEX_SSID },
  { flimflam::kWifiHiddenSsid, PROPERTY_INDEX_WIFI_HIDDEN_SSID },
  { flimflam::kWifiPhyMode, PROPERTY_INDEX_WIFI_PHY_MODE },
};

EnumMapper<ConnectionType>::Pair network_type_table[] = {
  { flimflam::kTypeEthernet, TYPE_ETHERNET },
  { flimflam::kTypeWifi, TYPE_WIFI },
  { flimflam::kTypeWimax, TYPE_WIMAX },
  { flimflam::kTypeBluetooth, TYPE_BLUETOOTH },
  { flimflam::kTypeCellular, TYPE_CELLULAR },
  { flimflam::kTypeVPN, TYPE_VPN },
};

EnumMapper<ConnectionSecurity>::Pair network_security_table[] = {
  { flimflam::kSecurityNone, SECURITY_NONE },
  { flimflam::kSecurityWep, SECURITY_WEP },
  { flimflam::kSecurityWpa, SECURITY_WPA },
  { flimflam::kSecurityRsn, SECURITY_RSN },
  { flimflam::kSecurityPsk, SECURITY_PSK },
  { flimflam::kSecurity8021x, SECURITY_8021X },
};

EnumMapper<EAPMethod>::Pair network_eap_method_table[] = {
  { flimflam::kEapMethodPEAP, EAP_METHOD_PEAP },
  { flimflam::kEapMethodTLS, EAP_METHOD_TLS },
  { flimflam::kEapMethodTTLS, EAP_METHOD_TTLS },
  { flimflam::kEapMethodLEAP, EAP_METHOD_LEAP },
};

EnumMapper<EAPPhase2Auth>::Pair network_eap_auth_table[] = {
  { flimflam::kEapPhase2AuthPEAPMD5, EAP_PHASE_2_AUTH_MD5 },
  { flimflam::kEapPhase2AuthPEAPMSCHAPV2, EAP_PHASE_2_AUTH_MSCHAPV2 },
  { flimflam::kEapPhase2AuthTTLSMD5, EAP_PHASE_2_AUTH_MD5 },
  { flimflam::kEapPhase2AuthTTLSMSCHAPV2, EAP_PHASE_2_AUTH_MSCHAPV2 },
  { flimflam::kEapPhase2AuthTTLSMSCHAP, EAP_PHASE_2_AUTH_MSCHAP },
  { flimflam::kEapPhase2AuthTTLSPAP, EAP_PHASE_2_AUTH_PAP },
  { flimflam::kEapPhase2AuthTTLSCHAP, EAP_PHASE_2_AUTH_CHAP },
};

EnumMapper<ProviderType>::Pair provider_type_table[] = {
  { flimflam::kProviderL2tpIpsec, PROVIDER_TYPE_L2TP_IPSEC_PSK },
  { flimflam::kProviderOpenVpn, PROVIDER_TYPE_OPEN_VPN },
};

// Serve the singleton mapper instance.
const EnumMapper<PropertyIndex>* get_native_mapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<PropertyIndex>, mapper,
      (property_index_table,
       arraysize(property_index_table),
       PROPERTY_INDEX_UNKNOWN));
  return &mapper;
}

}  // namespace

// -------------------- NativeNetworkDeviceParser --------------------

NativeNetworkDeviceParser::NativeNetworkDeviceParser()
    : NetworkDeviceParser(get_native_mapper()) {
}

NativeNetworkDeviceParser::~NativeNetworkDeviceParser() {
}

NetworkDevice* NativeNetworkDeviceParser::CreateNewNetworkDevice(
    const std::string& device_path) {
  NetworkDevice* device =
      NetworkDeviceParser::CreateNewNetworkDevice(device_path);
  device->SetNetworkDeviceParser(new NativeNetworkDeviceParser());
  return device;
}

bool NativeNetworkDeviceParser::ParseValue(
    PropertyIndex index, const base::Value& value, NetworkDevice* device) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value.GetAsString(&type_string)) {
        device->set_type(ParseType(type_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value.GetAsString(&name))
        return false;
      device->set_name(name);
      return true;
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value.GetAsString(&unique_id))
        return false;
      device->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_CARRIER: {
      std::string carrier;
      if (!value.GetAsString(&carrier))
        return false;
      device->set_carrier(carrier);
      return true;
    }
    case PROPERTY_INDEX_SCANNING: {
      bool scanning;
      if (!value.GetAsBoolean(&scanning))
        return false;
      device->set_scanning(scanning);
      return true;
    }
    case PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING: {
      bool data_roaming_allowed;
      if (!value.GetAsBoolean(&data_roaming_allowed))
        return false;
      device->set_data_roaming_allowed(data_roaming_allowed);
      return true;
    }
    case PROPERTY_INDEX_CELLULAR_APN_LIST:
      if (value.IsType(base::Value::TYPE_LIST)) {
        CellularApnList provider_apn_list;
        if (!ParseApnList(static_cast<const ListValue&>(value),
                          &provider_apn_list))
          return false;
        device->set_provider_apn_list(provider_apn_list);
        return true;
      }
      break;
    case PROPERTY_INDEX_NETWORKS:
      if (value.IsType(base::Value::TYPE_LIST)) {
        // Ignored.
        return true;
      }
      break;
    case PROPERTY_INDEX_FOUND_NETWORKS:
      if (value.IsType(base::Value::TYPE_LIST)) {
        CellularNetworkList found_cellular_networks;
        if (!ParseFoundNetworksFromList(
                static_cast<const ListValue&>(value),
                &found_cellular_networks))
          return false;
        device->set_found_cellular_networks(found_cellular_networks);
        return true;
      }
      break;
    case PROPERTY_INDEX_HOME_PROVIDER: {
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        const DictionaryValue& dict =
            static_cast<const DictionaryValue&>(value);
        std::string home_provider_code;
        std::string home_provider_country;
        std::string home_provider_name;
        dict.GetStringWithoutPathExpansion(flimflam::kOperatorCodeKey,
                                           &home_provider_code);
        dict.GetStringWithoutPathExpansion(flimflam::kOperatorCountryKey,
                                           &home_provider_country);
        dict.GetStringWithoutPathExpansion(flimflam::kOperatorNameKey,
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
    case PROPERTY_INDEX_PROVIDER_REQUIRES_ROAMING: {
      bool provider_requires_roaming;
      if (!value.GetAsBoolean(&provider_requires_roaming))
        return false;
      device->set_provider_requires_roaming(provider_requires_roaming);
      return true;
    }
    case PROPERTY_INDEX_MEID:
    case PROPERTY_INDEX_ICCID:
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
      if (!value.GetAsString(&item))
        return false;
      switch (index) {
        case PROPERTY_INDEX_MEID:
          device->set_meid(item);
          break;
        case PROPERTY_INDEX_ICCID:
          device->set_iccid(item);
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
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        SimLockState sim_lock_state;
        int sim_retries_left;
        bool sim_lock_enabled;
        if (!ParseSimLockStateFromDictionary(
                static_cast<const DictionaryValue&>(value),
                &sim_lock_state,
                &sim_retries_left,
                &sim_lock_enabled))
          return false;
        device->set_sim_lock_state(sim_lock_state);
        device->set_sim_retries_left(sim_retries_left);
        if (sim_lock_enabled)
          device->set_sim_pin_required(SIM_PIN_REQUIRED);
        else
          device->set_sim_pin_required(SIM_PIN_NOT_REQUIRED);
        return true;
      }
      break;
    case PROPERTY_INDEX_SIM_PRESENT: {
      bool sim_present;
      if (!value.GetAsBoolean(&sim_present))
        return false;
      device->set_sim_present(sim_present);
      return true;
    }
    case PROPERTY_INDEX_POWERED:
      // we don't care about the value, just the fact that it changed
      return true;
    case PROPERTY_INDEX_PRL_VERSION: {
      int prl_version;
      if (!value.GetAsInteger(&prl_version))
        return false;
      device->set_prl_version(prl_version);
      return true;
    }
    case PROPERTY_INDEX_SUPPORT_NETWORK_SCAN: {
      bool support_network_scan;
      if (!value.GetAsBoolean(&support_network_scan))
        return false;
      device->set_support_network_scan(support_network_scan);
      return true;
    }
    case PROPERTY_INDEX_SUPPORTED_CARRIERS: {
      if (value.IsType(base::Value::TYPE_LIST)) {
        device->set_supported_carriers(static_cast<const ListValue&>(value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_TECHNOLOGY_FAMILY: {
      std::string technology_family_string;
      if (value.GetAsString(&technology_family_string)) {
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
  return NativeNetworkParser::network_type_mapper()->Get(type);
}

bool NativeNetworkDeviceParser::ParseApnList(const ListValue& list,
                                             CellularApnList* apn_list) {
  apn_list->clear();
  apn_list->reserve(list.GetSize());
  for (ListValue::const_iterator it = list.begin(); it != list.end(); ++it) {
    if ((*it)->IsType(base::Value::TYPE_DICTIONARY)) {
      apn_list->resize(apn_list->size() + 1);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(*it);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnProperty, &apn_list->back().apn);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnNetworkIdProperty, &apn_list->back().network_id);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnUsernameProperty, &apn_list->back().username);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnPasswordProperty, &apn_list->back().password);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnNameProperty, &apn_list->back().name);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnLocalizedNameProperty,
          &apn_list->back().localized_name);
      dict->GetStringWithoutPathExpansion(
          flimflam::kApnLanguageProperty, &apn_list->back().language);
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
    if ((*it)->IsType(base::Value::TYPE_DICTIONARY)) {
      found_networks->resize(found_networks->size() + 1);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(*it);
      dict->GetStringWithoutPathExpansion(
          flimflam::kStatusProperty, &found_networks->back().status);
      dict->GetStringWithoutPathExpansion(
          flimflam::kNetworkIdProperty, &found_networks->back().network_id);
      dict->GetStringWithoutPathExpansion(
          flimflam::kShortNameProperty, &found_networks->back().short_name);
      dict->GetStringWithoutPathExpansion(
          flimflam::kLongNameProperty, &found_networks->back().long_name);
      dict->GetStringWithoutPathExpansion(
          flimflam::kTechnologyProperty, &found_networks->back().technology);
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
    { flimflam::kSIMLockPin, SIM_LOCKED_PIN },
    { flimflam::kSIMLockPuk, SIM_LOCKED_PUK },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<SimLockState>, parser,
      (table, arraysize(table), SIM_UNKNOWN));
  SimLockState parsed_state = parser.Get(state);
  DCHECK(parsed_state != SIM_UNKNOWN) << "Unknown SIMLock state encountered";
  return parsed_state;
}

bool NativeNetworkDeviceParser::ParseSimLockStateFromDictionary(
    const DictionaryValue& info,
    SimLockState* out_state,
    int* out_retries,
    bool* out_enabled) {
  std::string state_string;
  // Since RetriesLeft is sent as a uint32, which may overflow int32 range, from
  // Shill, it may be stored as an integer or a double in DictionaryValue.
  const base::Value* retries_value = NULL;
  if (!info.GetString(flimflam::kSIMLockTypeProperty, &state_string) ||
      !info.GetBoolean(flimflam::kSIMLockEnabledProperty, out_enabled) ||
      !info.Get(flimflam::kSIMLockRetriesLeftProperty, &retries_value) ||
      (retries_value->GetType() != base::Value::TYPE_INTEGER &&
       retries_value->GetType() != base::Value::TYPE_DOUBLE)) {
    LOG(ERROR) << "Error parsing SIMLock state";
    return false;
  }
  *out_state = ParseSimLockState(state_string);
  if (retries_value->GetType() == base::Value::TYPE_INTEGER) {
    retries_value->GetAsInteger(out_retries);
  } else if (retries_value->GetType() == base::Value::TYPE_DOUBLE) {
    double retries_double = 0;
    retries_value->GetAsDouble(&retries_double);
    *out_retries = retries_double;
  }
  return true;
}

TechnologyFamily NativeNetworkDeviceParser::ParseTechnologyFamily(
    const std::string& technology_family) {
  static EnumMapper<TechnologyFamily>::Pair table[] = {
    { flimflam::kTechnologyFamilyCdma, TECHNOLOGY_FAMILY_CDMA },
    { flimflam::kTechnologyFamilyGsm, TECHNOLOGY_FAMILY_GSM },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<TechnologyFamily>, parser,
      (table, arraysize(table), TECHNOLOGY_FAMILY_UNKNOWN));
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

// static
const EnumMapper<ConnectionType>* NativeNetworkParser::network_type_mapper() {
  CR_DEFINE_STATIC_LOCAL(
      EnumMapper<ConnectionType>,
      network_type_mapper,
      (network_type_table, arraysize(network_type_table), TYPE_UNKNOWN));
  return &network_type_mapper;
}

// static
const EnumMapper<ConnectionSecurity>*
    NativeNetworkParser::network_security_mapper() {
  CR_DEFINE_STATIC_LOCAL(
      EnumMapper<ConnectionSecurity>,
      network_security_mapper,
      (network_security_table, arraysize(network_security_table),
          SECURITY_UNKNOWN));
  return &network_security_mapper;
}

// static
const EnumMapper<EAPMethod>* NativeNetworkParser::network_eap_method_mapper() {
  CR_DEFINE_STATIC_LOCAL(
      EnumMapper<EAPMethod>,
      network_eap_method_mapper,
      (network_eap_method_table, arraysize(network_eap_method_table),
          EAP_METHOD_UNKNOWN));
  return &network_eap_method_mapper;
}

// static
const EnumMapper<EAPPhase2Auth>*
    NativeNetworkParser::network_eap_auth_mapper() {
  CR_DEFINE_STATIC_LOCAL(
      EnumMapper<EAPPhase2Auth>,
      network_eap_auth_mapper,
      (network_eap_auth_table, arraysize(network_eap_auth_table),
          EAP_PHASE_2_AUTH_AUTO));
  return &network_eap_auth_mapper;
}

const ConnectionType NativeNetworkParser::ParseConnectionType(
    const std::string& connection_type) {
  return network_type_mapper()->Get(connection_type);
}

Network* NativeNetworkParser::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  Network* network = NetworkParser::CreateNewNetwork(type, service_path);
  if (network) {
    if (type == TYPE_ETHERNET)
      network->SetNetworkParser(new NativeEthernetNetworkParser());
    else if (type == TYPE_WIFI)
      network->SetNetworkParser(new NativeWifiNetworkParser());
    else if (type == TYPE_WIMAX)
      network->SetNetworkParser(new NativeWimaxNetworkParser());
    else if (type == TYPE_CELLULAR)
      network->SetNetworkParser(new NativeCellularNetworkParser());
    else if (type == TYPE_VPN)
      network->SetNetworkParser(new NativeVirtualNetworkParser());
  }
  return network;
}

bool NativeNetworkParser::ParseValue(PropertyIndex index,
                                     const base::Value& value,
                                     Network* network) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value.GetAsString(&type_string)) {
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
      if (!value.GetAsString(&device_path))
        return false;
      network->set_device_path(device_path);
      return true;
    }
    case PROPERTY_INDEX_PROFILE: {
      // Note: currently this is only provided for non remembered networks.
      std::string profile_path;
      if (!value.GetAsString(&profile_path))
        return false;
      network->set_profile_path(profile_path);
      return true;
    }
    case PROPERTY_INDEX_STATE: {
      std::string state_string;
      if (value.GetAsString(&state_string)) {
        network->SetState(ParseState(state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MODE: {
      std::string mode_string;
      if (value.GetAsString(&mode_string)) {
        network->set_mode(ParseMode(mode_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ERROR: {
      std::string error_string;
      if (value.GetAsString(&error_string)) {
        network->SetError(ParseError(error_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CONNECTABLE: {
      bool connectable;
      if (!value.GetAsBoolean(&connectable))
        return false;
      network->set_connectable(connectable);
      return true;
    }
    case PROPERTY_INDEX_IS_ACTIVE: {
      bool is_active;
      if (!value.GetAsBoolean(&is_active))
        return false;
      network->set_is_active(is_active);
      return true;
    }
    case PROPERTY_INDEX_FAVORITE:
      // This property is ignored.
      return true;
    case PROPERTY_INDEX_SAVE_CREDENTIALS: {
      bool save_credentials;
      if (!value.GetAsBoolean(&save_credentials))
        return false;
      network->set_save_credentials(save_credentials);
      return true;
    }
    case PROPERTY_INDEX_CHECK_PORTAL:
      // This property is ignored.
      return true;
    default:
      return NetworkParser::ParseValue(index, value, network);
      break;
  }
  return false;
}

ConnectionType NativeNetworkParser::ParseType(const std::string& type) {
  return network_type_mapper()->Get(type);
}

ConnectionType NativeNetworkParser::ParseTypeFromDictionary(
    const DictionaryValue& info) {
  std::string type_string;
  info.GetString(flimflam::kTypeProperty, &type_string);
  return ParseType(type_string);
}

ConnectionMode NativeNetworkParser::ParseMode(const std::string& mode) {
  static EnumMapper<ConnectionMode>::Pair table[] = {
    { flimflam::kModeManaged, MODE_MANAGED },
    { flimflam::kModeAdhoc, MODE_ADHOC },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ConnectionMode>, parser,
      (table, arraysize(table), MODE_UNKNOWN));
  return parser.Get(mode);
}

ConnectionState NativeNetworkParser::ParseState(const std::string& state) {
  static EnumMapper<ConnectionState>::Pair table[] = {
    { flimflam::kStateIdle, STATE_IDLE },
    { flimflam::kStateCarrier, STATE_CARRIER },
    { flimflam::kStateAssociation, STATE_ASSOCIATION },
    { flimflam::kStateConfiguration, STATE_CONFIGURATION },
    { flimflam::kStateReady, STATE_READY },
    { flimflam::kStateDisconnect, STATE_DISCONNECT },
    { flimflam::kStateFailure, STATE_FAILURE },
    { flimflam::kStateActivationFailure, STATE_ACTIVATION_FAILURE },
    { flimflam::kStatePortal, STATE_PORTAL },
    { flimflam::kStateOnline, STATE_ONLINE },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ConnectionState>, parser,
      (table, arraysize(table), STATE_UNKNOWN));
  return parser.Get(state);
}

ConnectionError NativeNetworkParser::ParseError(const std::string& error) {
  static EnumMapper<ConnectionError>::Pair table[] = {
    { flimflam::kErrorOutOfRange, ERROR_OUT_OF_RANGE },
    { flimflam::kErrorPinMissing, ERROR_PIN_MISSING },
    { flimflam::kErrorDhcpFailed, ERROR_DHCP_FAILED },
    { flimflam::kErrorConnectFailed, ERROR_CONNECT_FAILED },
    { flimflam::kErrorBadPassphrase, ERROR_BAD_PASSPHRASE },
    { flimflam::kErrorBadWEPKey, ERROR_BAD_WEPKEY },
    { flimflam::kErrorActivationFailed, ERROR_ACTIVATION_FAILED },
    { flimflam::kErrorNeedEvdo, ERROR_NEED_EVDO },
    { flimflam::kErrorNeedHomeNetwork, ERROR_NEED_HOME_NETWORK },
    { flimflam::kErrorOtaspFailed, ERROR_OTASP_FAILED },
    { flimflam::kErrorAaaFailed, ERROR_AAA_FAILED },
    { flimflam::kErrorInternal, ERROR_INTERNAL },
    { flimflam::kErrorDNSLookupFailed, ERROR_DNS_LOOKUP_FAILED },
    { flimflam::kErrorHTTPGetFailed, ERROR_HTTP_GET_FAILED },
    { flimflam::kErrorIpsecPskAuthFailed, ERROR_IPSEC_PSK_AUTH_FAILED },
    { flimflam::kErrorIpsecCertAuthFailed, ERROR_IPSEC_CERT_AUTH_FAILED },
    { flimflam::kErrorPppAuthFailed, ERROR_PPP_AUTH_FAILED },
    { shill::kErrorEapAuthenticationFailed, ERROR_EAP_AUTHENTICATION_FAILED },
    { shill::kErrorEapLocalTlsFailed, ERROR_EAP_LOCAL_TLS_FAILED },
    { shill::kErrorEapRemoteTlsFailed, ERROR_EAP_REMOTE_TLS_FAILED },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ConnectionError>, parser,
      (table, arraysize(table), ERROR_NO_ERROR));
  return parser.Get(error);
}

// -------------------- NativeEthernetNetworkParser --------------------

NativeEthernetNetworkParser::NativeEthernetNetworkParser() {}
NativeEthernetNetworkParser::~NativeEthernetNetworkParser() {}

// -------------------- NativeWirelessNetworkParser --------------------

NativeWirelessNetworkParser::NativeWirelessNetworkParser() {}
NativeWirelessNetworkParser::~NativeWirelessNetworkParser() {}

bool NativeWirelessNetworkParser::ParseValue(PropertyIndex index,
                                             const base::Value& value,
                                             Network* network) {
  CHECK(network->type() == TYPE_WIFI ||
        network->type() == TYPE_WIMAX ||
        network->type() == TYPE_BLUETOOTH ||
        network->type() == TYPE_CELLULAR);
  WirelessNetwork* wireless_network = static_cast<WirelessNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_SIGNAL_STRENGTH: {
      int strength;
      if (!value.GetAsInteger(&strength))
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
                                             const base::Value& value,
                                             Network* network) {
  CHECK_EQ(TYPE_CELLULAR, network->type());
  CellularNetwork* cellular_network = static_cast<CellularNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_ACTIVATE_OVER_NON_CELLULAR_NETWORK: {
      bool activate_over_non_cellular_network;
      if (value.GetAsBoolean(&activate_over_non_cellular_network)) {
        cellular_network->set_activate_over_non_cellular_network(
            activate_over_non_cellular_network);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ACTIVATION_STATE: {
      std::string activation_state_string;
      if (value.GetAsString(&activation_state_string)) {
        cellular_network->set_activation_state(
            ParseActivationState(activation_state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_APN: {
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        cellular_network->set_apn(static_cast<const DictionaryValue&>(value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN: {
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        cellular_network->set_last_good_apn(
            static_cast<const DictionaryValue&>(value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NETWORK_TECHNOLOGY: {
      std::string network_technology_string;
      if (value.GetAsString(&network_technology_string)) {
        cellular_network->set_network_technology(
            ParseNetworkTechnology(network_technology_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ROAMING_STATE: {
      std::string roaming_state_string;
      if (value.GetAsString(&roaming_state_string)) {
        cellular_network->set_roaming_state(
            ParseRoamingState(roaming_state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_OPERATOR_NAME: {
      std::string value_str;
      if (!value.GetAsString(&value_str))
        break;
      cellular_network->set_operator_name(value_str);
      return true;
    }
    case PROPERTY_INDEX_OPERATOR_CODE: {
      std::string value_str;
      if (!value.GetAsString(&value_str))
        break;
      cellular_network->set_operator_code(value_str);
      return true;
    }
    case PROPERTY_INDEX_SERVING_OPERATOR: {
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        const DictionaryValue& dict =
            static_cast<const DictionaryValue&>(value);
        std::string value_str;
        dict.GetStringWithoutPathExpansion(
            flimflam::kOperatorNameKey, &value_str);
        cellular_network->set_operator_name(value_str);
        value_str.clear();
        dict.GetStringWithoutPathExpansion(
            flimflam::kOperatorCodeKey, &value_str);
        cellular_network->set_operator_code(value_str);
        value_str.clear();
        dict.GetStringWithoutPathExpansion(
            flimflam::kOperatorCountryKey, &value_str);
        cellular_network->set_operator_country(value_str);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_USAGE_URL: {
      std::string value_str;
      if (!value.GetAsString(&value_str))
        break;
      cellular_network->set_usage_url(value_str);
      return true;
    }
    case PROPERTY_INDEX_OLP: {
      if (value.IsType(base::Value::TYPE_DICTIONARY)) {
        const DictionaryValue& dict =
            static_cast<const DictionaryValue&>(value);
        std::string portal_url;
        std::string method;
        std::string postdata;
        dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalURL,
                                           &portal_url);
        dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalMethod,
                                           &method);
        dict.GetStringWithoutPathExpansion(flimflam::kPaymentPortalPostData,
                                           &postdata);
        cellular_network->set_payment_url(portal_url);
        cellular_network->set_post_data(postdata);
        cellular_network->set_using_post(
            LowerCaseEqualsASCII(method, kPostMethod));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_OLP_URL:
      // This property is ignored.
      return true;
    case PROPERTY_INDEX_STATE: {
      if (NativeWirelessNetworkParser::ParseValue(index, value, network))
        return true;
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
    { flimflam::kActivationStateActivated, ACTIVATION_STATE_ACTIVATED },
    { flimflam::kActivationStateActivating, ACTIVATION_STATE_ACTIVATING },
    { flimflam::kActivationStateNotActivated, ACTIVATION_STATE_NOT_ACTIVATED },
    { flimflam::kActivationStatePartiallyActivated,
      ACTIVATION_STATE_PARTIALLY_ACTIVATED},
    { flimflam::kActivationStateUnknown, ACTIVATION_STATE_UNKNOWN},
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ActivationState>, parser,
      (table, arraysize(table), ACTIVATION_STATE_UNKNOWN));
  return parser.Get(state);
}

NetworkTechnology NativeCellularNetworkParser::ParseNetworkTechnology(
    const std::string& technology) {
  static EnumMapper<NetworkTechnology>::Pair table[] = {
    { flimflam::kNetworkTechnology1Xrtt, NETWORK_TECHNOLOGY_1XRTT },
    { flimflam::kNetworkTechnologyEvdo, NETWORK_TECHNOLOGY_EVDO },
    { flimflam::kNetworkTechnologyGprs, NETWORK_TECHNOLOGY_GPRS },
    { flimflam::kNetworkTechnologyEdge, NETWORK_TECHNOLOGY_EDGE },
    { flimflam::kNetworkTechnologyUmts, NETWORK_TECHNOLOGY_UMTS },
    { flimflam::kNetworkTechnologyHspa, NETWORK_TECHNOLOGY_HSPA },
    { flimflam::kNetworkTechnologyHspaPlus, NETWORK_TECHNOLOGY_HSPA_PLUS },
    { flimflam::kNetworkTechnologyLte, NETWORK_TECHNOLOGY_LTE },
    { flimflam::kNetworkTechnologyLteAdvanced,
      NETWORK_TECHNOLOGY_LTE_ADVANCED },
    { flimflam::kNetworkTechnologyGsm, NETWORK_TECHNOLOGY_GSM },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<NetworkTechnology>, parser,
      (table, arraysize(table), NETWORK_TECHNOLOGY_UNKNOWN));
  return parser.Get(technology);
}

NetworkRoamingState NativeCellularNetworkParser::ParseRoamingState(
    const std::string& roaming_state) {
  static EnumMapper<NetworkRoamingState>::Pair table[] = {
    { flimflam::kRoamingStateHome, ROAMING_STATE_HOME },
    { flimflam::kRoamingStateRoaming, ROAMING_STATE_ROAMING },
    { flimflam::kRoamingStateUnknown, ROAMING_STATE_UNKNOWN },
  };
  CR_DEFINE_STATIC_LOCAL(EnumMapper<NetworkRoamingState>, parser,
      (table, arraysize(table), ROAMING_STATE_UNKNOWN));
  return parser.Get(roaming_state);
}

// -------------------- NativeWimaxNetworkParser --------------------

NativeWimaxNetworkParser::NativeWimaxNetworkParser() {}
NativeWimaxNetworkParser::~NativeWimaxNetworkParser() {}


bool NativeWimaxNetworkParser::ParseValue(PropertyIndex index,
                                          const base::Value& value,
                                          Network* network) {
  CHECK_EQ(TYPE_WIMAX, network->type());
  WimaxNetwork* wimax_network = static_cast<WimaxNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED: {
      bool passphrase_required;
      if (!value.GetAsBoolean(&passphrase_required))
        break;
      wimax_network->set_passphrase_required(passphrase_required);
      return true;
    }
    case PROPERTY_INDEX_EAP_IDENTITY: {
      std::string eap_identity;
      if (!value.GetAsString(&eap_identity))
        break;

      wimax_network->set_eap_identity(eap_identity);
    }
    case PROPERTY_INDEX_EAP_PASSWORD: {
      std::string passphrase;
      if (!value.GetAsString(&passphrase))
        break;

      wimax_network->set_eap_passphrase(passphrase);
      return true;
    }
    default:
      return NativeWirelessNetworkParser::ParseValue(index, value, network);
  }
  return false;
}

// -------------------- NativeWifiNetworkParser --------------------

NativeWifiNetworkParser::NativeWifiNetworkParser() {}
NativeWifiNetworkParser::~NativeWifiNetworkParser() {}

bool NativeWifiNetworkParser::ParseValue(PropertyIndex index,
                                         const base::Value& value,
                                         Network* network) {
  CHECK_EQ(TYPE_WIFI, network->type());
  WifiNetwork* wifi_network = static_cast<WifiNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_WIFI_HEX_SSID: {
      std::string ssid_hex;
      if (!value.GetAsString(&ssid_hex))
        return false;
      wifi_network->SetHexSsid(ssid_hex);
      return true;
    }
    case PROPERTY_INDEX_WIFI_BSSID: {
      std::string bssid;
      if (!value.GetAsString(&bssid))
        return false;
      wifi_network->set_bssid(bssid);
      return true;
    }
    case PROPERTY_INDEX_WIFI_HIDDEN_SSID: {
      bool hidden_ssid;
      if (!value.GetAsBoolean(&hidden_ssid))
        return false;
      wifi_network->set_hidden_ssid(hidden_ssid);
      return true;
    }
    case PROPERTY_INDEX_WIFI_FREQUENCY: {
      int frequency;
      if (!value.GetAsInteger(&frequency))
        return false;
      wifi_network->set_frequency(frequency);
      return true;
    }
    case PROPERTY_INDEX_NAME: {
      // Does not change network name when it was already set by WiFi.HexSSID.
      if (!wifi_network->name().empty())
        return true;
      else
        return NativeWirelessNetworkParser::ParseValue(index, value, network);
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value.GetAsString(&unique_id))
        break;
      wifi_network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_SECURITY: {
      std::string security_string;
      if (!value.GetAsString(&security_string))
        break;
      wifi_network->set_encryption(ParseSecurity(security_string));
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE: {
      std::string passphrase;
      if (!value.GetAsString(&passphrase))
        break;

      if (chromeos::UserManager::Get()->IsCurrentUserOwner())
        wifi_network->set_passphrase(passphrase);
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED: {
      bool passphrase_required;
      if (!value.GetAsBoolean(&passphrase_required))
        break;
      wifi_network->set_passphrase_required(passphrase_required);
      return true;
    }
    case PROPERTY_INDEX_IDENTITY: {
      std::string identity;
      if (!value.GetAsString(&identity))
        break;
      wifi_network->set_identity(identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_IDENTITY: {
      std::string eap_identity;
      if (!value.GetAsString(&eap_identity))
        break;
      wifi_network->set_eap_identity(eap_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_METHOD: {
      std::string eap_method;
      if (!value.GetAsString(&eap_method))
        break;
      wifi_network->set_eap_method(ParseEAPMethod(eap_method));
      return true;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      std::string eap_phase_2_auth;
      if (!value.GetAsString(&eap_phase_2_auth))
        break;
      wifi_network->set_eap_phase_2_auth(ParseEAPPhase2Auth(eap_phase_2_auth));
      return true;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY: {
      std::string eap_anonymous_identity;
      if (!value.GetAsString(&eap_anonymous_identity))
        break;
      wifi_network->set_eap_anonymous_identity(eap_anonymous_identity);
      return true;
    }
    case PROPERTY_INDEX_EAP_CERT_ID: {
      std::string eap_client_cert_pkcs11_id;
      if (!value.GetAsString(&eap_client_cert_pkcs11_id))
        break;
      wifi_network->set_eap_client_cert_pkcs11_id(eap_client_cert_pkcs11_id);
      return true;
    }
    case PROPERTY_INDEX_EAP_CA_CERT_NSS: {
      std::string eap_server_ca_cert_nss_nickname;
      if (!value.GetAsString(&eap_server_ca_cert_nss_nickname))
        break;
      wifi_network->set_eap_server_ca_cert_nss_nickname(
          eap_server_ca_cert_nss_nickname);
      return true;
    }
    case PROPERTY_INDEX_EAP_USE_SYSTEM_CAS: {
      bool eap_use_system_cas;
      if (!value.GetAsBoolean(&eap_use_system_cas))
        break;
      wifi_network->set_eap_use_system_cas(eap_use_system_cas);
      return true;
    }
    case PROPERTY_INDEX_EAP_PASSWORD: {
      std::string eap_passphrase;
      if (!value.GetAsString(&eap_passphrase))
        break;
      wifi_network->set_eap_passphrase(eap_passphrase);
      return true;
    }
    case PROPERTY_INDEX_EAP_CA_CERT: {
      std::string eap_cert_nickname;
      if (!value.GetAsString(&eap_cert_nickname))
        break;
      wifi_network->set_eap_server_ca_cert_nss_nickname(eap_cert_nickname);
      return true;
    }
    case PROPERTY_INDEX_WIFI_AUTH_MODE:
    case PROPERTY_INDEX_WIFI_PHY_MODE:
    case PROPERTY_INDEX_EAP_CLIENT_CERT:
    case PROPERTY_INDEX_EAP_CLIENT_CERT_NSS:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD:
    case PROPERTY_INDEX_EAP_KEY_ID:
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
  return network_security_mapper()->Get(security);
}

EAPMethod NativeWifiNetworkParser::ParseEAPMethod(const std::string& method) {
  return network_eap_method_mapper()->Get(method);
}

EAPPhase2Auth NativeWifiNetworkParser::ParseEAPPhase2Auth(
    const std::string& auth) {
  return network_eap_auth_mapper()->Get(auth);
}

// -------------------- NativeVirtualNetworkParser --------------------


NativeVirtualNetworkParser::NativeVirtualNetworkParser() {}
NativeVirtualNetworkParser::~NativeVirtualNetworkParser() {}

bool NativeVirtualNetworkParser::UpdateNetworkFromInfo(
    const DictionaryValue& info,
    Network* network) {
  CHECK_EQ(TYPE_VPN, network->type());
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
                                            const base::Value& value,
                                            Network* network) {
  CHECK_EQ(TYPE_VPN, network->type());
  VirtualNetwork* virtual_network = static_cast<VirtualNetwork*>(network);
  switch (index) {
    case PROPERTY_INDEX_PROVIDER: {
      // TODO(rkc): Figure out why is this ever not true and fix the root
      // cause. 'value' comes to us all the way from the cros dbus call, the
      // issue is likely on the cros side of things.
      if (value.GetType() != Value::TYPE_DICTIONARY)
        return false;

      const DictionaryValue& dict = static_cast<const DictionaryValue&>(value);
      for (DictionaryValue::key_iterator iter = dict.begin_keys();
           iter != dict.end_keys(); ++iter) {
        const std::string& key = *iter;
        const base::Value* provider_value;
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
                                                    const base::Value& value,
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
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS:
    case PROPERTY_INDEX_OPEN_VPN_CACERT: {
      std::string ca_cert_nss;
      if (!value.GetAsString(&ca_cert_nss))
        break;
      network->set_ca_cert_nss(ca_cert_nss);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PSK: {
      std::string psk_passphrase;
      if (!value.GetAsString(&psk_passphrase))
        break;
      network->set_psk_passphrase(psk_passphrase);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PSK_REQUIRED: {
      bool psk_passphrase_required;
      if (!value.GetAsBoolean(&psk_passphrase_required))
        break;
      network->set_psk_passphrase_required(psk_passphrase_required);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID:
    case PROPERTY_INDEX_OPEN_VPN_CLIENT_CERT_ID: {
      std::string client_cert_id;
      if (!value.GetAsString(&client_cert_id))
        break;
      network->set_client_cert_id(client_cert_id);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_USER:
    case PROPERTY_INDEX_OPEN_VPN_USER: {
      std::string username;
      if (!value.GetAsString(&username))
        break;
      network->set_username(username);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_PASSWORD:
    case PROPERTY_INDEX_OPEN_VPN_PASSWORD: {
      std::string user_passphrase;
      if (!value.GetAsString(&user_passphrase))
        break;
      network->set_user_passphrase(user_passphrase);
      return true;
    }
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED: {
      bool user_passphrase_required;
      if (!value.GetAsBoolean(&user_passphrase_required))
        break;
      network->set_user_passphrase_required(user_passphrase_required);
      return true;
    }
    case PROPERTY_INDEX_L2TPIPSEC_GROUP_NAME: {
      std::string group_name;
      if (!value.GetAsString(&group_name))
        break;
      network->set_group_name(group_name);
      return true;
    }
    default:
      break;
  }
  return false;
}

// static
const EnumMapper<ProviderType>*
    NativeVirtualNetworkParser::provider_type_mapper() {
  CR_DEFINE_STATIC_LOCAL(EnumMapper<ProviderType>, parser,
      (provider_type_table, arraysize(provider_type_table),
       PROVIDER_TYPE_MAX));
  return &parser;
}

ProviderType NativeVirtualNetworkParser::ParseProviderType(
    const std::string& type) {
  return provider_type_mapper()->Get(type);
}

}  // namespace chromeos
