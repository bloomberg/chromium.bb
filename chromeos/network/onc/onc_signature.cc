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
  { certificate::kCommonName, NULL, &kStringSignature },
  { certificate::kLocality, NULL, &kStringSignature },
  { certificate::kOrganization, NULL, &kStringSignature },
  { certificate::kOrganizationalUnit, NULL, &kStringSignature },
  { NULL }
};

// CertificatePattern is converted with function CreateUIData(...) to UIData
// stored in Shill.
const OncFieldSignature certificate_pattern_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { certificate::kEnrollmentURI, NULL, &kStringListSignature },
  { certificate::kIssuer, NULL, &kIssuerSubjectPatternSignature },
  { certificate::kIssuerCARef, NULL, &kStringListSignature },
  { certificate::kSubject, NULL, &kIssuerSubjectPatternSignature },
  { NULL }
};

const OncFieldSignature eap_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { eap::kAnonymousIdentity, flimflam::kEapAnonymousIdentityProperty,
    &kStringSignature },
  { eap::kClientCertPattern, NULL, &kCertificatePatternSignature },
  { eap::kClientCertRef, NULL, &kStringSignature },
  { eap::kClientCertType, NULL, &kStringSignature },
  { eap::kIdentity, flimflam::kEapIdentityProperty, &kStringSignature },
  // This field is converted during translation, see onc_translator_*.
  { eap::kInner, NULL, &kStringSignature },
  // This field is converted during translation, see onc_translator_*.
  { eap::kOuter, NULL, &kStringSignature },
  { eap::kPassword, flimflam::kEapPasswordProperty, &kStringSignature },
  { eap::kSaveCredentials, flimflam::kSaveCredentialsProperty,
    &kBoolSignature },
  { eap::kServerCARef, flimflam::kEapCaCertNssProperty, &kStringSignature },
  { eap::kUseSystemCAs, flimflam::kEapUseSystemCasProperty, &kBoolSignature },
  { NULL }
};

const OncFieldSignature ipsec_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  // Ignored by Shill, not necessary to synchronize.
  // Would be: flimflam::kL2tpIpsecAuthenticationType
  { vpn::kAuthenticationType, NULL, &kStringSignature },
  { vpn::kClientCertPattern, NULL, &kCertificatePatternSignature },
  { vpn::kClientCertRef, NULL, &kStringSignature },
  { vpn::kClientCertType, NULL, &kStringSignature },
  { vpn::kGroup, flimflam::kL2tpIpsecGroupNameProperty, &kStringSignature },
  // Ignored by Shill, not necessary to synchronize.
  // Would be: flimflam::kL2tpIpsecIkeVersion
  { vpn::kIKEVersion, NULL, &kIntegerSignature },
  { vpn::kPSK, flimflam::kL2tpIpsecPskProperty, &kStringSignature },
  { vpn::kSaveCredentials, flimflam::kSaveCredentialsProperty,
    &kBoolSignature },
  { vpn::kServerCARef, flimflam::kL2tpIpsecCaCertNssProperty,
    &kStringSignature },
  // Not yet supported.
  //  { vpn::kEAP, NULL, &kEAPSignature },
  //  { vpn::kXAUTH, NULL, &kXAUTHSignature },
  { NULL }
};

const OncFieldSignature l2tp_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { vpn::kPassword, flimflam::kL2tpIpsecPasswordProperty, &kStringSignature },
  // We don't synchronize l2tp's SaveCredentials field for now, as Shill doesn't
  // support separate settings for ipsec and l2tp.
  { vpn::kSaveCredentials, NULL, &kBoolSignature },
  { vpn::kUsername, flimflam::kL2tpIpsecUserProperty, &kStringSignature },
  { NULL }
};

const OncFieldSignature openvpn_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { vpn::kAuth, flimflam::kOpenVPNAuthProperty, &kStringSignature },
  { vpn::kAuthNoCache, flimflam::kOpenVPNAuthNoCacheProperty, &kBoolSignature },
  { vpn::kAuthRetry, flimflam::kOpenVPNAuthRetryProperty, &kStringSignature },
  { vpn::kCipher, flimflam::kOpenVPNCipherProperty, &kStringSignature },
  { vpn::kClientCertPattern, NULL, &kCertificatePatternSignature },
  { vpn::kClientCertRef, NULL, &kStringSignature },
  { vpn::kClientCertType, NULL, &kStringSignature },
  { vpn::kCompLZO, flimflam::kOpenVPNCompLZOProperty, &kStringSignature },
  { vpn::kCompNoAdapt, flimflam::kOpenVPNCompNoAdaptProperty, &kBoolSignature },
  { vpn::kKeyDirection, flimflam::kOpenVPNKeyDirectionProperty,
    &kStringSignature },
  { vpn::kNsCertType, flimflam::kOpenVPNNsCertTypeProperty, &kStringSignature },
  { vpn::kPassword, flimflam::kOpenVPNPasswordProperty, &kStringSignature },
  { vpn::kPort, flimflam::kOpenVPNPortProperty, &kIntegerSignature },
  { vpn::kProto, flimflam::kOpenVPNProtoProperty, &kStringSignature },
  { vpn::kPushPeerInfo, flimflam::kOpenVPNPushPeerInfoProperty,
    &kBoolSignature },
  { vpn::kRemoteCertEKU, flimflam::kOpenVPNRemoteCertEKUProperty,
    &kStringSignature },
  // This field is converted during translation, see onc_translator_*.
  { vpn::kRemoteCertKU, NULL, &kStringListSignature },
  { vpn::kRemoteCertTLS, flimflam::kOpenVPNRemoteCertTLSProperty,
    &kStringSignature },
  { vpn::kRenegSec, flimflam::kOpenVPNRenegSecProperty, &kIntegerSignature },
  { vpn::kSaveCredentials, flimflam::kSaveCredentialsProperty,
    &kBoolSignature },
  { vpn::kServerCARef, flimflam::kOpenVPNCaCertNSSProperty, &kStringSignature },
  // Not supported, yet.
  { vpn::kServerCertRef, NULL, &kStringSignature },
  { vpn::kServerPollTimeout, flimflam::kOpenVPNServerPollTimeoutProperty,
    &kIntegerSignature },
  { vpn::kShaper, flimflam::kOpenVPNShaperProperty, &kIntegerSignature },
  { vpn::kStaticChallenge, flimflam::kOpenVPNStaticChallengeProperty,
    &kStringSignature },
  { vpn::kTLSAuthContents, flimflam::kOpenVPNTLSAuthContentsProperty,
    &kStringSignature },
  { vpn::kTLSRemote, flimflam::kOpenVPNTLSRemoteProperty, &kStringSignature },
  { vpn::kUsername, flimflam::kOpenVPNUserProperty, &kStringSignature },
  // Not supported, yet.
  { vpn::kVerb, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature vpn_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { vpn::kHost, flimflam::kProviderHostProperty, &kStringSignature },
  { vpn::kIPsec, NULL, &kIPsecSignature },
  { vpn::kL2TP, NULL, &kL2TPSignature },
  { vpn::kOpenVPN, NULL, &kOpenVPNSignature },
  // This field is converted during translation, see onc_translator_*.
  { vpn::kType, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature ethernet_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  // Not supported, yet.
  { ethernet::kAuthentication, NULL, &kStringSignature },
  { ethernet::kEAP, NULL, &kEAPSignature },
  { NULL }
};

// Not supported, yet.
const OncFieldSignature ipconfig_fields[] = {
  { ipconfig::kGateway, NULL, &kStringSignature },
  { ipconfig::kIPAddress, NULL, &kStringSignature },
  { kNameServers, NULL, &kStringSignature },
  { ipconfig::kRoutingPrefix, NULL, &kIntegerSignature },
  { kSearchDomains, NULL, &kStringListSignature },
  { ipconfig::kType, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature proxy_location_fields[] = {
  { proxy::kHost, NULL, &kStringSignature },
  { proxy::kPort, NULL, &kIntegerSignature },
  { NULL }
};

const OncFieldSignature proxy_manual_fields[] = {
  { proxy::kFtp, NULL, &kProxyLocationSignature },
  { proxy::kHttp, NULL, &kProxyLocationSignature },
  { proxy::kHttps, NULL, &kProxyLocationSignature },
  { proxy::kSocks, NULL, &kProxyLocationSignature },
  { NULL }
};

// Proxy settings are converted to Shill by
// function ConvertOncProxySettingsToProxyConfig(...).
const OncFieldSignature proxy_settings_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { proxy::kExcludeDomains, NULL, &kStringListSignature },
  { proxy::kManual, NULL, &kProxyManualSignature },
  { proxy::kPAC, NULL, &kStringSignature },
  { proxy::kType, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature wifi_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { wifi::kAutoConnect, flimflam::kAutoConnectProperty, &kBoolSignature },
  { wifi::kEAP, NULL, &kEAPSignature },
  { wifi::kHiddenSSID, flimflam::kWifiHiddenSsid, &kBoolSignature },
  { wifi::kPassphrase, flimflam::kPassphraseProperty, &kStringSignature },
  { wifi::kSSID, flimflam::kSSIDProperty, &kStringSignature },
  // This field is converted during translation, see onc_translator_*.
  { wifi::kSecurity, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature network_configuration_fields[] = {
  { kRecommended, NULL, &kRecommendedSignature },
  { kEthernet, NULL, &kEthernetSignature },
  { kGUID, flimflam::kGuidProperty, &kStringSignature },
  { kIPConfigs, NULL, &kIPConfigListSignature },
  // Shill doesn't allow setting the name for non-VPN networks.
  // This field is conditionally translated, see onc_translator_*.
  { kName, NULL, &kStringSignature },
  // Not supported, yet.
  { kNameServers, NULL, &kStringListSignature },
  { kProxySettings, NULL, &kProxySettingsSignature },
  // No need to translate.
  { kRemove, NULL, &kBoolSignature },
  // Not supported, yet.
  { kSearchDomains, NULL, &kStringListSignature },
  // This field is converted during translation, see onc_translator_*.
  { kType, NULL, &kStringSignature },
  { kVPN, NULL, &kVPNSignature },
  { kWiFi, NULL, &kWiFiSignature },
  { NULL }
};

// Certificates are not translated to Shill.
const OncFieldSignature certificate_fields[] = {
  { kGUID, NULL, &kStringSignature },
  { certificate::kPKCS12, NULL, &kStringSignature },
  { kRemove, NULL, &kBoolSignature },
  { certificate::kTrust, NULL, &kStringListSignature },
  { certificate::kType, NULL, &kStringSignature },
  { certificate::kX509, NULL, &kStringSignature },
  { NULL }
};

const OncFieldSignature toplevel_configuration_fields[] = {
  { kCertificates, NULL, &kCertificateListSignature },
  { kNetworkConfigurations, NULL, &kNetworkConfigurationListSignature },
  { kType, NULL, &kStringSignature },
  { encrypted::kCipher, NULL, &kStringSignature },
  { encrypted::kCiphertext, NULL, &kStringSignature },
  { encrypted::kHMAC, NULL, &kStringSignature },
  { encrypted::kHMACMethod, NULL, &kStringSignature },
  { encrypted::kIV, NULL, &kStringSignature },
  { encrypted::kIterations, NULL, &kIntegerSignature },
  { encrypted::kSalt, NULL, &kStringSignature },
  { encrypted::kStretch, NULL, &kStringSignature },
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

const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name) {
  if (!signature.fields)
    return NULL;
  for (const OncFieldSignature* field_signature = signature.fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    if (onc_field_name == field_signature->onc_field_name)
      return field_signature;
  }
  return NULL;
}

}  // namespace onc
}  // namespace chromeos
