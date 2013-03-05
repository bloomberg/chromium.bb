// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_signature.h"

#include "chromeos/network/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using base::Value;

namespace chromeos {
namespace onc {
namespace {

const OncValueSignature kBoolSignature = {
  Value::TYPE_BOOLEAN, NULL
};
const OncValueSignature kStringSignature = {
  Value::TYPE_STRING, NULL
};
const OncValueSignature kIntegerSignature = {
  Value::TYPE_INTEGER, NULL
};
const OncValueSignature kStringListSignature = {
  Value::TYPE_LIST, NULL, &kStringSignature
};
const OncValueSignature kIPConfigListSignature = {
  Value::TYPE_LIST, NULL, &kIPConfigSignature
};

const OncFieldSignature issuer_subject_pattern_fields[] = {
  { certificate::kCommonName, &kStringSignature },
  { certificate::kLocality, &kStringSignature },
  { certificate::kOrganization, &kStringSignature },
  { certificate::kOrganizationalUnit, &kStringSignature },
  { NULL }
};

const OncFieldSignature certificate_pattern_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { certificate::kEnrollmentURI, &kStringListSignature },
  { certificate::kIssuer, &kIssuerSubjectPatternSignature },
  { certificate::kIssuerCARef, &kStringListSignature },
  { certificate::kSubject, &kIssuerSubjectPatternSignature },
  { NULL }
};

const OncFieldSignature eap_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { eap::kAnonymousIdentity, &kStringSignature },
  { eap::kClientCertPattern, &kCertificatePatternSignature },
  { eap::kClientCertRef, &kStringSignature },
  { eap::kClientCertType, &kStringSignature },
  { eap::kIdentity, &kStringSignature },
  { eap::kInner, &kStringSignature },
  { eap::kOuter, &kStringSignature },
  { eap::kPassword, &kStringSignature },
  { eap::kSaveCredentials, &kBoolSignature },
  { eap::kServerCARef, &kStringSignature },
  { eap::kUseSystemCAs, &kBoolSignature },
  { NULL }
};

const OncFieldSignature ipsec_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { vpn::kAuthenticationType, &kStringSignature },
  { vpn::kClientCertPattern, &kCertificatePatternSignature },
  { vpn::kClientCertRef, &kStringSignature },
  { vpn::kClientCertType, &kStringSignature },
  { vpn::kGroup, &kStringSignature },
  { vpn::kIKEVersion, &kIntegerSignature },
  { vpn::kPSK, &kStringSignature },
  { vpn::kSaveCredentials, &kBoolSignature },
  { vpn::kServerCARef, &kStringSignature },
  // Not yet supported.
  //  { vpn::kEAP, &kEAPSignature },
  //  { vpn::kXAUTH, &kXAUTHSignature },
  { NULL }
};

const OncFieldSignature l2tp_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { vpn::kPassword, &kStringSignature },
  { vpn::kSaveCredentials, &kBoolSignature },
  { vpn::kUsername, &kStringSignature },
  { NULL }
};

const OncFieldSignature openvpn_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { vpn::kAuth, &kStringSignature },
  { vpn::kAuthNoCache, &kBoolSignature },
  { vpn::kAuthRetry, &kStringSignature },
  { vpn::kCipher, &kStringSignature },
  { vpn::kClientCertPattern, &kCertificatePatternSignature },
  { vpn::kClientCertRef, &kStringSignature },
  { vpn::kClientCertType, &kStringSignature },
  { vpn::kCompLZO, &kStringSignature },
  { vpn::kCompNoAdapt, &kBoolSignature },
  { vpn::kKeyDirection, &kStringSignature },
  { vpn::kNsCertType, &kStringSignature },
  { vpn::kPassword, &kStringSignature },
  { vpn::kPort, &kIntegerSignature },
  { vpn::kProto, &kStringSignature },
  { vpn::kPushPeerInfo, &kBoolSignature },
  { vpn::kRemoteCertEKU, &kStringSignature },
  { vpn::kRemoteCertKU, &kStringListSignature },
  { vpn::kRemoteCertTLS, &kStringSignature },
  { vpn::kRenegSec, &kIntegerSignature },
  { vpn::kSaveCredentials, &kBoolSignature },
  { vpn::kServerCARef, &kStringSignature },
  // Not supported, yet.
  { vpn::kServerCertRef, &kStringSignature },
  { vpn::kServerPollTimeout, &kIntegerSignature },
  { vpn::kShaper, &kIntegerSignature },
  { vpn::kStaticChallenge, &kStringSignature },
  { vpn::kTLSAuthContents, &kStringSignature },
  { vpn::kTLSRemote, &kStringSignature },
  { vpn::kUsername, &kStringSignature },
  // Not supported, yet.
  { vpn::kVerb, &kStringSignature },
  { NULL }
};

const OncFieldSignature vpn_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { vpn::kAutoConnect, &kBoolSignature },
  { vpn::kHost, &kStringSignature },
  { vpn::kIPsec, &kIPsecSignature },
  { vpn::kL2TP, &kL2TPSignature },
  { vpn::kOpenVPN, &kOpenVPNSignature },
  { vpn::kType, &kStringSignature },
  { NULL }
};

const OncFieldSignature ethernet_fields[] = {
  { kRecommended, &kRecommendedSignature },
  // Not supported, yet.
  { ethernet::kAuthentication, &kStringSignature },
  { ethernet::kEAP, &kEAPSignature },
  { NULL }
};

// Not supported, yet.
const OncFieldSignature ipconfig_fields[] = {
  { ipconfig::kGateway, &kStringSignature },
  { ipconfig::kIPAddress, &kStringSignature },
  { network_config::kNameServers, &kStringSignature },
  { ipconfig::kRoutingPrefix, &kIntegerSignature },
  { network_config::kSearchDomains, &kStringListSignature },
  { ipconfig::kType, &kStringSignature },
  { NULL }
};

const OncFieldSignature proxy_location_fields[] = {
  { proxy::kHost, &kStringSignature },
  { proxy::kPort, &kIntegerSignature },
  { NULL }
};

const OncFieldSignature proxy_manual_fields[] = {
  { proxy::kFtp, &kProxyLocationSignature },
  { proxy::kHttp, &kProxyLocationSignature },
  { proxy::kHttps, &kProxyLocationSignature },
  { proxy::kSocks, &kProxyLocationSignature },
  { NULL }
};

const OncFieldSignature proxy_settings_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { proxy::kExcludeDomains, &kStringListSignature },
  { proxy::kManual, &kProxyManualSignature },
  { proxy::kPAC, &kStringSignature },
  { proxy::kType, &kStringSignature },
  { NULL }
};

const OncFieldSignature wifi_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { wifi::kAutoConnect, &kBoolSignature },
  { wifi::kEAP, &kEAPSignature },
  { wifi::kHiddenSSID, &kBoolSignature },
  { wifi::kPassphrase, &kStringSignature },
  { wifi::kSSID, &kStringSignature },
  { wifi::kSecurity, &kStringSignature },
  { NULL }
};

const OncFieldSignature wifi_with_state_fields[] = {
  { wifi::kBSSID, &kStringSignature },
  { wifi::kSignalStrength, &kIntegerSignature },
  { NULL }
};

const OncFieldSignature cellular_with_state_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { cellular::kActivateOverNonCellularNetwork, &kStringSignature },
  { cellular::kActivationState, &kStringSignature },
  { cellular::kAllowRoaming, &kStringSignature },
  { cellular::kAPN, &kStringSignature },
  { cellular::kCarrier, &kStringSignature },
  { cellular::kESN, &kStringSignature },
  { cellular::kFamily, &kStringSignature },
  { cellular::kFirmwareRevision, &kStringSignature },
  { cellular::kFoundNetworks, &kStringSignature },
  { cellular::kHardwareRevision, &kStringSignature },
  { cellular::kHomeProvider, &kStringSignature },
  { cellular::kICCID, &kStringSignature },
  { cellular::kIMEI, &kStringSignature },
  { cellular::kIMSI, &kStringSignature },
  { cellular::kManufacturer, &kStringSignature },
  { cellular::kMDN, &kStringSignature },
  { cellular::kMEID, &kStringSignature },
  { cellular::kMIN, &kStringSignature },
  { cellular::kModelID, &kStringSignature },
  { cellular::kNetworkTechnology, &kStringSignature },
  { cellular::kOperatorCode, &kStringSignature },
  { cellular::kOperatorName, &kStringSignature },
  { cellular::kPRLVersion, &kStringSignature },
  { cellular::kProviderRequiresRoaming, &kStringSignature },
  { cellular::kRoamingState, &kStringSignature },
  { cellular::kSelectedNetwork, &kStringSignature },
  { cellular::kServingOperator, &kStringSignature },
  { cellular::kSIMLockStatus, &kStringSignature },
  { cellular::kSIMPresent, &kStringSignature },
  { cellular::kSupportedCarriers, &kStringSignature },
  { cellular::kSupportNetworkScan, &kStringSignature },
  { NULL }
};

const OncFieldSignature network_configuration_fields[] = {
  { kRecommended, &kRecommendedSignature },
  { network_config::kEthernet, &kEthernetSignature },
  { network_config::kGUID, &kStringSignature },
  // Not supported, yet.
  { network_config::kIPConfigs, &kIPConfigListSignature },
  { network_config::kName, &kStringSignature },
  // Not supported, yet.
  { network_config::kNameServers, &kStringListSignature },
  { network_config::kProxySettings, &kProxySettingsSignature },
  { kRemove, &kBoolSignature },
  // Not supported, yet.
  { network_config::kSearchDomains, &kStringListSignature },
  { network_config::kType, &kStringSignature },
  { network_config::kVPN, &kVPNSignature },
  { network_config::kWiFi, &kWiFiSignature },
  { NULL }
};

const OncFieldSignature network_with_state_fields[] = {
  { network_config::kCellular, &kCellularWithStateSignature },
  { network_config::kConnectionState, &kStringSignature },
  { network_config::kWiFi, &kWiFiWithStateSignature },
  { NULL }
};

const OncFieldSignature certificate_fields[] = {
  { certificate::kGUID, &kStringSignature },
  { certificate::kPKCS12, &kStringSignature },
  { kRemove, &kBoolSignature },
  { certificate::kTrust, &kStringListSignature },
  { certificate::kType, &kStringSignature },
  { certificate::kX509, &kStringSignature },
  { NULL }
};

const OncFieldSignature toplevel_configuration_fields[] = {
  { toplevel_config::kCertificates, &kCertificateListSignature },
  { toplevel_config::kNetworkConfigurations,
    &kNetworkConfigurationListSignature },
  { toplevel_config::kType, &kStringSignature },
  { encrypted::kCipher, &kStringSignature },
  { encrypted::kCiphertext, &kStringSignature },
  { encrypted::kHMAC, &kStringSignature },
  { encrypted::kHMACMethod, &kStringSignature },
  { encrypted::kIV, &kStringSignature },
  { encrypted::kIterations, &kIntegerSignature },
  { encrypted::kSalt, &kStringSignature },
  { encrypted::kStretch, &kStringSignature },
  { NULL }
};

}  // namespace

const OncValueSignature kRecommendedSignature = {
  Value::TYPE_LIST, NULL, &kStringSignature
};
const OncValueSignature kEAPSignature = {
  Value::TYPE_DICTIONARY, eap_fields, NULL
};
const OncValueSignature kIssuerSubjectPatternSignature = {
  Value::TYPE_DICTIONARY, issuer_subject_pattern_fields, NULL
};
const OncValueSignature kCertificatePatternSignature = {
  Value::TYPE_DICTIONARY, certificate_pattern_fields, NULL
};
const OncValueSignature kIPsecSignature = {
  Value::TYPE_DICTIONARY, ipsec_fields, NULL
};
const OncValueSignature kL2TPSignature = {
  Value::TYPE_DICTIONARY, l2tp_fields, NULL
};
const OncValueSignature kOpenVPNSignature = {
  Value::TYPE_DICTIONARY, openvpn_fields, NULL
};
const OncValueSignature kVPNSignature = {
  Value::TYPE_DICTIONARY, vpn_fields, NULL
};
const OncValueSignature kEthernetSignature = {
  Value::TYPE_DICTIONARY, ethernet_fields, NULL
};
const OncValueSignature kIPConfigSignature = {
  Value::TYPE_DICTIONARY, ipconfig_fields, NULL
};
const OncValueSignature kProxyLocationSignature = {
  Value::TYPE_DICTIONARY, proxy_location_fields, NULL
};
const OncValueSignature kProxyManualSignature = {
  Value::TYPE_DICTIONARY, proxy_manual_fields, NULL
};
const OncValueSignature kProxySettingsSignature = {
  Value::TYPE_DICTIONARY, proxy_settings_fields, NULL
};
const OncValueSignature kWiFiSignature = {
  Value::TYPE_DICTIONARY, wifi_fields, NULL
};
const OncValueSignature kCertificateSignature = {
  Value::TYPE_DICTIONARY, certificate_fields, NULL
};
const OncValueSignature kNetworkConfigurationSignature = {
  Value::TYPE_DICTIONARY, network_configuration_fields, NULL
};
const OncValueSignature kCertificateListSignature = {
  Value::TYPE_LIST, NULL, &kCertificateSignature
};
const OncValueSignature kNetworkConfigurationListSignature = {
  Value::TYPE_LIST, NULL, &kNetworkConfigurationSignature
};
const OncValueSignature kToplevelConfigurationSignature = {
  Value::TYPE_DICTIONARY, toplevel_configuration_fields, NULL
};

// Derived "ONC with State" signatures.
const OncValueSignature kNetworkWithStateSignature = {
  Value::TYPE_DICTIONARY, network_with_state_fields, NULL,
  &kNetworkConfigurationSignature
};
const OncValueSignature kWiFiWithStateSignature = {
  Value::TYPE_DICTIONARY, wifi_with_state_fields, NULL, &kWiFiSignature
};
const OncValueSignature kCellularWithStateSignature = {
  Value::TYPE_DICTIONARY, cellular_with_state_fields, NULL
};

const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name) {
  if (!signature.fields)
    return NULL;
  for (const OncFieldSignature* field_signature = signature.fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    if (onc_field_name == field_signature->onc_field_name)
      return field_signature;
  }
  if (signature.base_signature)
    return GetFieldSignature(*signature.base_signature, onc_field_name);
  return NULL;
}

}  // namespace onc
}  // namespace chromeos
