// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translation_tables.h"

#include <cstddef>

#include "base/logging.h"
#include "chromeos/network/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

// CertificatePattern is converted with function CreateUIData(...) to UIData
// stored in Shill.
//
// Proxy settings are converted to Shill by function
// ConvertOncProxySettingsToProxyConfig(...).
//
// Translation of IPConfig objects is not supported, yet.

namespace {

const FieldTranslationEntry eap_fields[] = {
  { eap::kAnonymousIdentity, shill::kEapAnonymousIdentityProperty },
  { eap::kIdentity, shill::kEapIdentityProperty },
  // This field is converted during translation, see onc_translator_*.
  // { eap::kInner, shill::kEapPhase2AuthProperty },

  // This field is converted during translation, see onc_translator_*.
  // { eap::kOuter, shill::kEapMethodProperty },
  { eap::kPassword, shill::kEapPasswordProperty },
  { eap::kSaveCredentials, shill::kSaveCredentialsProperty },
  { eap::kServerCAPEMs, shill::kEapCaCertPemProperty },
  { eap::kUseSystemCAs, shill::kEapUseSystemCasProperty },
  { NULL }
};

const FieldTranslationEntry ipsec_fields[] = {
  // Ignored by Shill, not necessary to synchronize.
  // { ipsec::kAuthenticationType, shill::kL2tpIpsecAuthenticationType },
  { ipsec::kGroup, shill::kL2tpIpsecTunnelGroupProperty },
  // Ignored by Shill, not necessary to synchronize.
  // { ipsec::kIKEVersion, shill::kL2tpIpsecIkeVersion },
  { ipsec::kPSK, shill::kL2tpIpsecPskProperty },
  { vpn::kSaveCredentials, shill::kSaveCredentialsProperty },
  { ipsec::kServerCAPEMs, shill::kL2tpIpsecCaCertPemProperty },
  { NULL }
};

const FieldTranslationEntry l2tp_fields[] = {
  { vpn::kPassword, shill::kL2tpIpsecPasswordProperty },
  // We don't synchronize l2tp's SaveCredentials field for now, as Shill doesn't
  // support separate settings for ipsec and l2tp.
  // { vpn::kSaveCredentials, &kBoolSignature },
  { vpn::kUsername, shill::kL2tpIpsecUserProperty },
  { NULL }
};

const FieldTranslationEntry openvpn_fields[] = {
  { openvpn::kAuth, shill::kOpenVPNAuthProperty },
  { openvpn::kAuthNoCache, shill::kOpenVPNAuthNoCacheProperty },
  { openvpn::kAuthRetry, shill::kOpenVPNAuthRetryProperty },
  { openvpn::kCipher, shill::kOpenVPNCipherProperty },
  { openvpn::kCompLZO, shill::kOpenVPNCompLZOProperty },
  { openvpn::kCompNoAdapt, shill::kOpenVPNCompNoAdaptProperty },
  { openvpn::kKeyDirection, shill::kOpenVPNKeyDirectionProperty },
  { openvpn::kNsCertType, shill::kOpenVPNNsCertTypeProperty },
  { vpn::kPassword, shill::kOpenVPNPasswordProperty },
  { openvpn::kPort, shill::kOpenVPNPortProperty },
  { openvpn::kProto, shill::kOpenVPNProtoProperty },
  { openvpn::kPushPeerInfo, shill::kOpenVPNPushPeerInfoProperty },
  { openvpn::kRemoteCertEKU, shill::kOpenVPNRemoteCertEKUProperty },
  // This field is converted during translation, see onc_translator_*.
  // { openvpn::kRemoteCertKU, shill::kOpenVPNRemoteCertKUProperty },
  { openvpn::kRemoteCertTLS, shill::kOpenVPNRemoteCertTLSProperty },
  { openvpn::kRenegSec, shill::kOpenVPNRenegSecProperty },
  { vpn::kSaveCredentials, shill::kSaveCredentialsProperty },
  { openvpn::kServerCAPEMs, shill::kOpenVPNCaCertPemProperty },
  { openvpn::kServerPollTimeout, shill::kOpenVPNServerPollTimeoutProperty },
  { openvpn::kShaper, shill::kOpenVPNShaperProperty },
  { openvpn::kStaticChallenge, shill::kOpenVPNStaticChallengeProperty },
  { openvpn::kTLSAuthContents, shill::kOpenVPNTLSAuthContentsProperty },
  { openvpn::kTLSRemote, shill::kOpenVPNTLSRemoteProperty },
  { vpn::kUsername, shill::kOpenVPNUserProperty },
  { NULL }
};

const FieldTranslationEntry vpn_fields[] = {
  { vpn::kAutoConnect, shill::kAutoConnectProperty },
  { vpn::kHost, shill::kProviderHostProperty },
  // This field is converted during translation, see onc_translator_*.
  // { vpn::kType, shill::kProviderTypeProperty },
  { NULL }
};

const FieldTranslationEntry wifi_fields[] = {
  { wifi::kAutoConnect, shill::kAutoConnectProperty },
  { wifi::kBSSID, shill::kWifiBSsid },
  { wifi::kFrequency, shill::kWifiFrequency },
  { wifi::kFrequencyList, shill::kWifiFrequencyListProperty },
  { wifi::kHiddenSSID, shill::kWifiHiddenSsid },
  { wifi::kPassphrase, shill::kPassphraseProperty },
  { wifi::kSSID, shill::kSSIDProperty },
  // This field is converted during translation, see onc_translator_*.
  // { wifi::kSecurity, shill::kSecurityProperty },
  { wifi::kSignalStrength, shill::kSignalStrengthProperty },
  { NULL }
};

const FieldTranslationEntry cellular_apn_fields[] = {
  { cellular_apn::kName, shill::kApnProperty },
  { cellular_apn::kUsername, shill::kApnUsernameProperty },
  { cellular_apn::kPassword, shill::kApnPasswordProperty },
  { NULL }
};

const FieldTranslationEntry cellular_provider_fields[] = {
  { cellular_provider::kCode, shill::kOperatorCodeKey },
  { cellular_provider::kCountry, shill::kOperatorCountryKey },
  { cellular_provider::kName, shill::kOperatorNameKey },
  { NULL }
};

const FieldTranslationEntry cellular_fields[] = {
  { cellular::kActivateOverNonCellularNetwork,
    shill::kActivateOverNonCellularNetworkProperty },
  { cellular::kActivationState, shill::kActivationStateProperty },
  { cellular::kAllowRoaming, shill::kCellularAllowRoamingProperty },
  { cellular::kCarrier, shill::kCarrierProperty },
  { cellular::kESN, shill::kEsnProperty },
  { cellular::kFamily, shill::kTechnologyFamilyProperty },
  { cellular::kFirmwareRevision, shill::kFirmwareRevisionProperty },
  { cellular::kFoundNetworks, shill::kFoundNetworksProperty },
  { cellular::kHardwareRevision, shill::kHardwareRevisionProperty },
  { cellular::kICCID, shill::kIccidProperty },
  { cellular::kIMEI, shill::kImeiProperty },
  { cellular::kIMSI, shill::kImsiProperty },
  { cellular::kManufacturer, shill::kManufacturerProperty },
  { cellular::kMDN, shill::kMdnProperty },
  { cellular::kMEID, shill::kMeidProperty },
  { cellular::kMIN, shill::kMinProperty },
  { cellular::kModelID, shill::kModelIDProperty },
  { cellular::kNetworkTechnology, shill::kNetworkTechnologyProperty },
  { cellular::kPRLVersion, shill::kPRLVersionProperty },
  { cellular::kProviderRequiresRoaming,
    shill::kProviderRequiresRoamingProperty },
  { cellular::kRoamingState, shill::kRoamingStateProperty },
  { cellular::kSelectedNetwork, shill::kSelectedNetworkProperty },
  { cellular::kSIMLockStatus, shill::kSIMLockStatusProperty },
  { cellular::kSIMPresent, shill::kSIMPresentProperty },
  { cellular::kSupportedCarriers, shill::kSupportedCarriersProperty },
  { cellular::kSupportNetworkScan, shill::kSupportNetworkScanProperty },
  { NULL }
};

const FieldTranslationEntry network_fields[] = {
  // Shill doesn't allow setting the name for non-VPN networks.
  // This field is conditionally translated, see onc_translator_*.
  // { network_config::kName, shill::kNameProperty },
  { network_config::kGUID, shill::kGuidProperty },
  // This field is converted during translation, see onc_translator_*.
  // { network_config::kType, shill::kTypeProperty },

  // This field is converted during translation, see
  // onc_translator_shill_to_onc.cc. It is only converted when going from
  // Shill->ONC, and ignored otherwise.
  // { network_config::kConnectionState, shill::kStateProperty },
  { NULL }
};

struct OncValueTranslationEntry {
  const OncValueSignature* onc_signature;
  const FieldTranslationEntry* field_translation_table;
};

const OncValueTranslationEntry onc_value_translation_table[] = {
  { &kEAPSignature, eap_fields },
  { &kIPsecSignature, ipsec_fields },
  { &kL2TPSignature, l2tp_fields },
  { &kOpenVPNSignature, openvpn_fields },
  { &kVPNSignature, vpn_fields },
  { &kWiFiSignature, wifi_fields },
  { &kWiFiWithStateSignature, wifi_fields },
  { &kCellularApnSignature, cellular_apn_fields },
  { &kCellularProviderSignature, cellular_provider_fields },
  { &kCellularSignature, cellular_fields },
  { &kCellularWithStateSignature, cellular_fields },
  { &kNetworkWithStateSignature, network_fields },
  { &kNetworkConfigurationSignature, network_fields },
  { NULL }
};

struct NestedShillDictionaryEntry {
  const OncValueSignature* onc_signature;
  // NULL terminated list of Shill property keys.
  const char* const* shill_property_path;
};

const char* cellular_apn_property_path_entries[] = {
  shill::kCellularApnProperty,
  NULL
};

const NestedShillDictionaryEntry nested_shill_dictionaries[] = {
  { &kCellularApnSignature, cellular_apn_property_path_entries },
  { NULL }
};

}  // namespace

const StringTranslationEntry kNetworkTypeTable[] = {
  // This mapping is ensured in the translation code.
  //  { network_type::kEthernet, shill::kTypeEthernet },
  //  { network_type::kEthernet, shill::kTypeEthernetEap },
  { network_type::kWiFi, shill::kTypeWifi },
  { network_type::kCellular, shill::kTypeCellular },
  { network_type::kVPN, shill::kTypeVPN },
  { NULL }
};

const StringTranslationEntry kVPNTypeTable[] = {
  { vpn::kTypeL2TP_IPsec, shill::kProviderL2tpIpsec },
  { vpn::kOpenVPN, shill::kProviderOpenVpn },
  { NULL }
};

// The first matching line is chosen.
const StringTranslationEntry kWiFiSecurityTable[] = {
  { wifi::kNone, shill::kSecurityNone },
  { wifi::kWEP_PSK, shill::kSecurityWep },
  { wifi::kWPA_PSK, shill::kSecurityPsk },
  { wifi::kWPA_EAP, shill::kSecurity8021x },
  { wifi::kWPA_PSK, shill::kSecurityRsn },
  { wifi::kWPA_PSK, shill::kSecurityWpa },
  { NULL }
};

const StringTranslationEntry kEAPOuterTable[] = {
  { eap::kPEAP, shill::kEapMethodPEAP },
  { eap::kEAP_TLS, shill::kEapMethodTLS },
  { eap::kEAP_TTLS, shill::kEapMethodTTLS },
  { eap::kLEAP, shill::kEapMethodLEAP },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == PEAP
const StringTranslationEntry kEAP_PEAP_InnerTable[] = {
  { eap::kMD5, shill::kEapPhase2AuthPEAPMD5 },
  { eap::kMSCHAPv2, shill::kEapPhase2AuthPEAPMSCHAPV2 },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == TTLS
const StringTranslationEntry kEAP_TTLS_InnerTable[] = {
  { eap::kMD5, shill::kEapPhase2AuthTTLSMD5 },
  { eap::kMSCHAPv2, shill::kEapPhase2AuthTTLSMSCHAPV2 },
  { eap::kPAP, shill::kEapPhase2AuthTTLSPAP },
  { NULL }
};

const FieldTranslationEntry* GetFieldTranslationTable(
    const OncValueSignature& onc_signature) {
  for (const OncValueTranslationEntry* it = onc_value_translation_table;
       it->onc_signature != NULL; ++it) {
    if (it->onc_signature == &onc_signature)
      return it->field_translation_table;
  }
  return NULL;
}

std::vector<std::string> GetPathToNestedShillDictionary(
    const OncValueSignature& onc_signature) {
  std::vector<std::string> shill_property_path;
  for (const NestedShillDictionaryEntry* it = nested_shill_dictionaries;
       it->onc_signature != NULL; ++it) {
    if (it->onc_signature == &onc_signature) {
      for (const char* const* key = it->shill_property_path; *key != NULL;
           ++key) {
        shill_property_path.push_back(std::string(*key));
      }
      break;
    }
  }
  return shill_property_path;
}

bool GetShillPropertyName(const std::string& onc_field_name,
                          const FieldTranslationEntry table[],
                          std::string* shill_property_name) {
  for (const FieldTranslationEntry* it = table;
       it->onc_field_name != NULL; ++it) {
    if (it->onc_field_name != onc_field_name)
      continue;
    *shill_property_name = it->shill_property_name;
    return true;
  }
  return false;
}

bool TranslateStringToShill(const StringTranslationEntry table[],
                            const std::string& onc_value,
                            std::string* shill_value) {
  for (int i = 0; table[i].onc_value != NULL; ++i) {
    if (onc_value != table[i].onc_value)
      continue;
    *shill_value = table[i].shill_value;
    return true;
  }
  LOG(ERROR) << "Value '" << onc_value << "' cannot be translated to Shill";
  return false;
}

bool TranslateStringToONC(const StringTranslationEntry table[],
                          const std::string& shill_value,
                          std::string* onc_value) {
  for (int i = 0; table[i].shill_value != NULL; ++i) {
    if (shill_value != table[i].shill_value)
      continue;
    *onc_value = table[i].onc_value;
    return true;
  }
  LOG(ERROR) << "Value '" << shill_value << "' cannot be translated to ONC";
  return false;
}

}  // namespace onc
}  // namespace chromeos
