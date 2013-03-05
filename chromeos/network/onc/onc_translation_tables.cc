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
  { eap::kAnonymousIdentity, flimflam::kEapAnonymousIdentityProperty },
  { eap::kIdentity, flimflam::kEapIdentityProperty },
  // This field is converted during translation, see onc_translator_*.
  // { eap::kInner, flimflam::kEapPhase2AuthProperty },

  // This field is converted during translation, see onc_translator_*.
  // { eap::kOuter, flimflam::kEapMethodProperty },
  { eap::kPassword, flimflam::kEapPasswordProperty },
  { eap::kSaveCredentials, flimflam::kSaveCredentialsProperty },
  { eap::kServerCARef, flimflam::kEapCaCertNssProperty },
  { eap::kUseSystemCAs, flimflam::kEapUseSystemCasProperty },
  { NULL }
};

const FieldTranslationEntry ipsec_fields[] = {
  // Ignored by Shill, not necessary to synchronize.
  // { vpn::kAuthenticationType, flimflam::kL2tpIpsecAuthenticationType },
  { vpn::kGroup, flimflam::kL2tpIpsecGroupNameProperty },
  // Ignored by Shill, not necessary to synchronize.
  // { vpn::kIKEVersion, flimflam::kL2tpIpsecIkeVersion },
  { vpn::kPSK, flimflam::kL2tpIpsecPskProperty },
  { vpn::kSaveCredentials, flimflam::kSaveCredentialsProperty },
  { vpn::kServerCARef, flimflam::kL2tpIpsecCaCertNssProperty },
  { NULL }
};

const FieldTranslationEntry l2tp_fields[] = {
  { vpn::kPassword, flimflam::kL2tpIpsecPasswordProperty },
  // We don't synchronize l2tp's SaveCredentials field for now, as Shill doesn't
  // support separate settings for ipsec and l2tp.
  // { vpn::kSaveCredentials, &kBoolSignature },
  { vpn::kUsername, flimflam::kL2tpIpsecUserProperty },
  { NULL }
};

const FieldTranslationEntry openvpn_fields[] = {
  { vpn::kAuth, flimflam::kOpenVPNAuthProperty },
  { vpn::kAuthNoCache, flimflam::kOpenVPNAuthNoCacheProperty },
  { vpn::kAuthRetry, flimflam::kOpenVPNAuthRetryProperty },
  { vpn::kCipher, flimflam::kOpenVPNCipherProperty },
  { vpn::kCompLZO, flimflam::kOpenVPNCompLZOProperty },
  { vpn::kCompNoAdapt, flimflam::kOpenVPNCompNoAdaptProperty },
  { vpn::kKeyDirection, flimflam::kOpenVPNKeyDirectionProperty },
  { vpn::kNsCertType, flimflam::kOpenVPNNsCertTypeProperty },
  { vpn::kPassword, flimflam::kOpenVPNPasswordProperty },
  { vpn::kPort, flimflam::kOpenVPNPortProperty },
  { vpn::kProto, flimflam::kOpenVPNProtoProperty },
  { vpn::kPushPeerInfo, flimflam::kOpenVPNPushPeerInfoProperty },
  { vpn::kRemoteCertEKU, flimflam::kOpenVPNRemoteCertEKUProperty },
  // This field is converted during translation, see onc_translator_*.
  // { vpn::kRemoteCertKU, flimflam::kOpenVPNRemoteCertKUProperty },
  { vpn::kRemoteCertTLS, flimflam::kOpenVPNRemoteCertTLSProperty },
  { vpn::kRenegSec, flimflam::kOpenVPNRenegSecProperty },
  { vpn::kSaveCredentials, flimflam::kSaveCredentialsProperty },
  { vpn::kServerCARef, flimflam::kOpenVPNCaCertNSSProperty },
  { vpn::kServerPollTimeout, flimflam::kOpenVPNServerPollTimeoutProperty },
  { vpn::kShaper, flimflam::kOpenVPNShaperProperty },
  { vpn::kStaticChallenge, flimflam::kOpenVPNStaticChallengeProperty },
  { vpn::kTLSAuthContents, flimflam::kOpenVPNTLSAuthContentsProperty },
  { vpn::kTLSRemote, flimflam::kOpenVPNTLSRemoteProperty },
  { vpn::kUsername, flimflam::kOpenVPNUserProperty },
  { NULL }
};

const FieldTranslationEntry vpn_fields[] = {
  { vpn::kAutoConnect, flimflam::kAutoConnectProperty },
  { vpn::kHost, flimflam::kProviderHostProperty },
  // This field is converted during translation, see onc_translator_*.
  // { vpn::kType, flimflam::kProviderTypeProperty },
  { NULL }
};

const FieldTranslationEntry wifi_fields[] = {
  { wifi::kAutoConnect, flimflam::kAutoConnectProperty },
  { wifi::kBSSID, flimflam::kWifiBSsid },
  { wifi::kHiddenSSID, flimflam::kWifiHiddenSsid },
  { wifi::kPassphrase, flimflam::kPassphraseProperty },
  { wifi::kSSID, flimflam::kSSIDProperty },
  // This field is converted during translation, see onc_translator_*.
  // { wifi::kSecurity, flimflam::kSecurityProperty },
  { wifi::kSignalStrength, flimflam::kSignalStrengthProperty },
  { NULL }
};

const FieldTranslationEntry cellular_with_state_fields[] = {
  { cellular::kActivateOverNonCellularNetwork,
    shill::kActivateOverNonCellularNetworkProperty },
  { cellular::kActivationState, flimflam::kActivationStateProperty },
  { cellular::kAllowRoaming, flimflam::kCellularAllowRoamingProperty },
  { cellular::kAPN, flimflam::kApnProperty },
  { cellular::kCarrier, flimflam::kCarrierProperty },
  { cellular::kESN, flimflam::kEsnProperty },
  { cellular::kFamily, flimflam::kTechnologyFamilyProperty },
  { cellular::kFirmwareRevision, flimflam::kFirmwareRevisionProperty },
  { cellular::kFoundNetworks, flimflam::kFoundNetworksProperty },
  { cellular::kHardwareRevision, flimflam::kHardwareRevisionProperty },
  { cellular::kHomeProvider, flimflam::kHomeProviderProperty },
  { cellular::kICCID, flimflam::kIccidProperty },
  { cellular::kIMEI, flimflam::kImeiProperty },
  { cellular::kIMSI, flimflam::kImsiProperty },
  { cellular::kManufacturer, flimflam::kManufacturerProperty },
  { cellular::kMDN, flimflam::kMdnProperty },
  { cellular::kMEID, flimflam::kMeidProperty },
  { cellular::kMIN, flimflam::kMinProperty },
  { cellular::kModelID, flimflam::kModelIDProperty },
  { cellular::kNetworkTechnology, flimflam::kNetworkTechnologyProperty },
  { cellular::kOperatorCode, flimflam::kOperatorCodeProperty },
  { cellular::kOperatorName, flimflam::kOperatorNameProperty },
  { cellular::kPRLVersion, flimflam::kPRLVersionProperty },
  { cellular::kProviderRequiresRoaming,
    shill::kProviderRequiresRoamingProperty },
  { cellular::kRoamingState, flimflam::kRoamingStateProperty },
  { cellular::kSelectedNetwork, flimflam::kSelectedNetworkProperty },
  { cellular::kServingOperator, flimflam::kServingOperatorProperty },
  { cellular::kSIMLockStatus, flimflam::kSIMLockStatusProperty },
  { cellular::kSIMPresent, shill::kSIMPresentProperty },
  { cellular::kSupportedCarriers, shill::kSupportedCarriersProperty },
  { cellular::kSupportNetworkScan, flimflam::kSupportNetworkScanProperty },
  { NULL }
};

const FieldTranslationEntry network_fields[] = {
  // Shill doesn't allow setting the name for non-VPN networks.
  // This field is conditionally translated, see onc_translator_*.
  // { network_config::kName, flimflam::kNameProperty },
  { network_config::kGUID, flimflam::kGuidProperty },
  // This field is converted during translation, see onc_translator_*.
  // { network_config::kType, flimflam::kTypeProperty },

  // This field is converted during translation, see
  // onc_translator_shill_to_onc.cc. It is only converted when going from
  // Shill->ONC, and ignored otherwise.
  // { network_config::kConnectionState, flimflam::kStateProperty },
  { NULL }
};

const OncValueTranslationEntry onc_value_translation_table[] = {
  { &kEAPSignature, eap_fields },
  { &kIPsecSignature, ipsec_fields },
  { &kL2TPSignature, l2tp_fields },
  { &kOpenVPNSignature, openvpn_fields },
  { &kVPNSignature, vpn_fields },
  { &kWiFiSignature, wifi_fields },
  { &kWiFiWithStateSignature, wifi_fields },
  { &kCellularWithStateSignature, cellular_with_state_fields },
  { &kNetworkWithStateSignature, network_fields },
  { &kNetworkConfigurationSignature, network_fields },
  { NULL }
};

}  // namespace

const StringTranslationEntry kNetworkTypeTable[] = {
  { network_type::kEthernet, flimflam::kTypeEthernet },
  { network_type::kWiFi, flimflam::kTypeWifi },
  { network_type::kCellular, flimflam::kTypeCellular },
  { network_type::kVPN, flimflam::kTypeVPN },
  { NULL }
};

const StringTranslationEntry kVPNTypeTable[] = {
  { vpn::kTypeL2TP_IPsec, flimflam::kProviderL2tpIpsec },
  { vpn::kOpenVPN, flimflam::kProviderOpenVpn },
  { NULL }
};

const StringTranslationEntry kWiFiSecurityTable[] = {
  { wifi::kNone, flimflam::kSecurityNone },
  { wifi::kWEP_PSK, flimflam::kSecurityWep },
  { wifi::kWPA_PSK, flimflam::kSecurityPsk },
  { wifi::kWPA_EAP, flimflam::kSecurity8021x },
  { NULL }
};

const StringTranslationEntry kEAPOuterTable[] = {
  { eap::kPEAP, flimflam::kEapMethodPEAP },
  { eap::kEAP_TLS, flimflam::kEapMethodTLS },
  { eap::kEAP_TTLS, flimflam::kEapMethodTTLS },
  { eap::kLEAP, flimflam::kEapMethodLEAP },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == PEAP
const StringTranslationEntry kEAP_PEAP_InnerTable[] = {
  { eap::kMD5, flimflam::kEapPhase2AuthPEAPMD5 },
  { eap::kMSCHAPv2, flimflam::kEapPhase2AuthPEAPMSCHAPV2 },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == TTLS
const StringTranslationEntry kEAP_TTLS_InnerTable[] = {
  { eap::kMD5, flimflam::kEapPhase2AuthTTLSMD5 },
  { eap::kMSCHAPv2, flimflam::kEapPhase2AuthTTLSMSCHAPV2 },
  { eap::kPAP, flimflam::kEapPhase2AuthTTLSPAP },
  { NULL }
};

const FieldTranslationEntry* GetFieldTranslationTable(
    const OncValueSignature& onc_signature) {
  for (const OncValueTranslationEntry* it = onc_value_translation_table;
       it->onc_signature != NULL; ++it) {
    if (it->onc_signature != &onc_signature)
      continue;
    return it->field_translation_table;
  }
  return NULL;
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
