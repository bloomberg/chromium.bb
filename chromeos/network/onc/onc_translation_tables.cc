// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translation_tables.h"

#include <cstddef>

#include "base/logging.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/network/tether_constants.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

// CertificatePattern is converted with function CreateUIData(...) to UIData
// stored in Shill.

namespace {

const FieldTranslationEntry eap_fields[] = {
    {::onc::eap::kAnonymousIdentity, shill::kEapAnonymousIdentityProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::client_cert::kClientCertPKCS11Id, shill::kEapCertIdProperty },
    {::onc::eap::kIdentity, shill::kEapIdentityProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::eap::kInner, shill::kEapPhase2AuthProperty },
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::eap::kOuter, shill::kEapMethodProperty },
    {::onc::eap::kPassword, shill::kEapPasswordProperty},
    {::onc::eap::kSaveCredentials, shill::kSaveCredentialsProperty},
    {::onc::eap::kServerCAPEMs, shill::kEapCaCertPemProperty},
    {::onc::eap::kSubjectMatch, shill::kEapSubjectMatchProperty},
    // This field is converted during translation, see onc_translator_*.
    // {::onc::eap::kSubjectAlternativeNameMatch,
    //  shill::kEapSubjectAlternativeNameMatchProperty},
    {::onc::eap::kTLSVersionMax, shill::kEapTLSVersionMaxProperty},
    {::onc::eap::kUseSystemCAs, shill::kEapUseSystemCasProperty},
    {::onc::eap::kUseProactiveKeyCaching,
     shill::kEapUseProactiveKeyCachingProperty},
    {nullptr}};

const FieldTranslationEntry ipsec_fields[] = {
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::ipsec::kAuthenticationType, shill::kL2tpIpsecAuthenticationType
    // },
    // {::onc::client_cert::kClientCertPKCS11Id,
    //  shill::kL2tpIpsecClientCertIdProperty},
    {::onc::ipsec::kGroup, shill::kL2tpIpsecTunnelGroupProperty},
    // Ignored by Shill, not necessary to synchronize.
    // { ::onc::ipsec::kIKEVersion, shill::kL2tpIpsecIkeVersion },
    {::onc::ipsec::kPSK, shill::kL2tpIpsecPskProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::vpn::kSaveCredentials, shill::kSaveCredentialsProperty},
    {::onc::ipsec::kServerCAPEMs, shill::kL2tpIpsecCaCertPemProperty},
    {nullptr}};

const FieldTranslationEntry xauth_fields[] = {
    {::onc::vpn::kPassword, shill::kL2tpIpsecXauthPasswordProperty},
    {::onc::vpn::kUsername, shill::kL2tpIpsecXauthUserProperty},
    {nullptr}};

const FieldTranslationEntry l2tp_fields[] = {
    {::onc::l2tp::kPassword, shill::kL2tpIpsecPasswordProperty},
    // We don't synchronize l2tp's SaveCredentials field for now, as Shill
    // doesn't support separate settings for ipsec and l2tp.
    // { ::onc::l2tp::kSaveCredentials, &kBoolSignature },
    {::onc::l2tp::kUsername, shill::kL2tpIpsecUserProperty},
    // kLcpEchoDisabled is a bool in ONC and a string in Shill.
    // {::onc::l2tp::kLcpEchoDisabled,
    // shill::kL2tpIpsecLcpEchoDisabledProperty},
    {nullptr}};

const FieldTranslationEntry openvpn_fields[] = {
    {::onc::openvpn::kAuth, shill::kOpenVPNAuthProperty},
    {::onc::openvpn::kAuthNoCache, shill::kOpenVPNAuthNoCacheProperty},
    {::onc::openvpn::kAuthRetry, shill::kOpenVPNAuthRetryProperty},
    {::onc::openvpn::kCipher, shill::kOpenVPNCipherProperty},
    // This field is converted during translation, see onc_translator_*.
    // {::onc::client_cert::kClientCertPKCS11Id,
    //  shill::kOpenVPNClientCertIdProperty},
    {::onc::openvpn::kCompLZO, shill::kOpenVPNCompLZOProperty},
    {::onc::openvpn::kCompNoAdapt, shill::kOpenVPNCompNoAdaptProperty},
    // This field is converted during translation, see onc_translator_*
    // {::onc::openvpn::kCompressionAlgorithm, shill::kOpenVPNCompressProperty},
    {::onc::openvpn::kExtraHosts, shill::kOpenVPNExtraHostsProperty},
    {::onc::openvpn::kIgnoreDefaultRoute,
     shill::kOpenVPNIgnoreDefaultRouteProperty},
    {::onc::openvpn::kKeyDirection, shill::kOpenVPNKeyDirectionProperty},
    {::onc::openvpn::kNsCertType, shill::kOpenVPNNsCertTypeProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::vpn::kOTP, shill::kOpenVPNTokenProperty or kOpenVPNOTPProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::vpn::kPassword, shill::kOpenVPNPasswordProperty},
    {::onc::openvpn::kPort, shill::kOpenVPNPortProperty},
    {::onc::openvpn::kProto, shill::kOpenVPNProtoProperty},
    {::onc::openvpn::kPushPeerInfo, shill::kOpenVPNPushPeerInfoProperty},
    {::onc::openvpn::kRemoteCertEKU, shill::kOpenVPNRemoteCertEKUProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::openvpn::kRemoteCertKU, shill::kOpenVPNRemoteCertKUProperty },
    {::onc::openvpn::kRemoteCertTLS, shill::kOpenVPNRemoteCertTLSProperty},
    {::onc::openvpn::kRenegSec, shill::kOpenVPNRenegSecProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::vpn::kSaveCredentials, shill::kSaveCredentialsProperty},
    {::onc::openvpn::kServerCAPEMs, shill::kOpenVPNCaCertPemProperty},
    {::onc::openvpn::kServerPollTimeout,
     shill::kOpenVPNServerPollTimeoutProperty},
    {::onc::openvpn::kShaper, shill::kOpenVPNShaperProperty},
    {::onc::openvpn::kStaticChallenge, shill::kOpenVPNStaticChallengeProperty},
    {::onc::openvpn::kTLSAuthContents, shill::kOpenVPNTLSAuthContentsProperty},
    {::onc::openvpn::kTLSRemote, shill::kOpenVPNTLSRemoteProperty},
    {::onc::openvpn::kTLSVersionMin, shill::kOpenVPNTLSVersionMinProperty},
    {::onc::vpn::kUsername, shill::kOpenVPNUserProperty},
    {::onc::openvpn::kVerb, shill::kOpenVPNVerbProperty},
    {::onc::openvpn::kVerifyHash, shill::kOpenVPNVerifyHashProperty},
    {nullptr}};

const FieldTranslationEntry arc_vpn_fields[] = {
    {::onc::arc_vpn::kTunnelChrome, shill::kArcVpnTunnelChromeProperty},
    {nullptr}};

const FieldTranslationEntry verify_x509_fields[] = {
    {::onc::verify_x509::kName, shill::kOpenVPNVerifyX509NameProperty},
    {::onc::verify_x509::kType, shill::kOpenVPNVerifyX509TypeProperty},
    {nullptr}};

const FieldTranslationEntry vpn_fields[] = {
    {::onc::vpn::kAutoConnect, shill::kAutoConnectProperty},
    // These fields are converted during translation, see onc_translator_*.
    // { ::onc::vpn::kHost, shill::kProviderHostProperty},
    // { ::onc::vpn::kType, shill::kProviderTypeProperty },
    {nullptr}};

const FieldTranslationEntry tether_fields[] = {
    {::onc::tether::kBatteryPercentage, kTetherBatteryPercentage},
    {::onc::tether::kCarrier, kTetherCarrier},
    {::onc::tether::kHasConnectedToHost, kTetherHasConnectedToHost},
    {::onc::tether::kSignalStrength, kTetherSignalStrength},
    {nullptr}};

const FieldTranslationEntry wifi_fields[] = {
    {::onc::wifi::kAutoConnect, shill::kAutoConnectProperty},
    {::onc::wifi::kBSSID, shill::kWifiBSsid},
    // This dictionary is converted during translation, see onc_translator_*.
    // { ::onc::wifi::kEAP, shill::kEap*},
    {::onc::wifi::kFrequency, shill::kWifiFrequency},
    {::onc::wifi::kFrequencyList, shill::kWifiFrequencyListProperty},
    {::onc::wifi::kFTEnabled, shill::kWifiFTEnabled},
    {::onc::wifi::kHexSSID, shill::kWifiHexSsid},
    {::onc::wifi::kHiddenSSID, shill::kWifiHiddenSsid},
    {::onc::wifi::kPassphrase, shill::kPassphraseProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::wifi::kSecurity, shill::kSecurityClassProperty },
    {::onc::wifi::kSignalStrength, shill::kSignalStrengthProperty},
    {::onc::wifi::kTetheringState, shill::kTetheringProperty},
    {nullptr}};

const FieldTranslationEntry cellular_apn_fields[] = {
    {::onc::cellular_apn::kAccessPointName, shill::kApnProperty},
    {::onc::cellular_apn::kName, shill::kApnNameProperty},
    {::onc::cellular_apn::kUsername, shill::kApnUsernameProperty},
    {::onc::cellular_apn::kPassword, shill::kApnPasswordProperty},
    {::onc::cellular_apn::kAuthentication, shill::kApnAuthenticationProperty},
    {::onc::cellular_apn::kLocalizedName, shill::kApnLocalizedNameProperty},
    {::onc::cellular_apn::kLanguage, shill::kApnLanguageProperty},
    {nullptr}};

const FieldTranslationEntry cellular_found_network_fields[] = {
    {::onc::cellular_found_network::kNetworkId, shill::kNetworkIdProperty},
    {::onc::cellular_found_network::kStatus, shill::kStatusProperty},
    {::onc::cellular_found_network::kTechnology, shill::kTechnologyProperty},
    {::onc::cellular_found_network::kShortName, shill::kShortNameProperty},
    {::onc::cellular_found_network::kLongName, shill::kLongNameProperty},
    {nullptr}};

const FieldTranslationEntry cellular_payment_portal_fields[] = {
    {::onc::cellular_payment_portal::kMethod, shill::kPaymentPortalMethod},
    {::onc::cellular_payment_portal::kPostData, shill::kPaymentPortalPostData},
    {::onc::cellular_payment_portal::kUrl, shill::kPaymentPortalURL},
    {nullptr}};

const FieldTranslationEntry cellular_provider_fields[] = {
    {::onc::cellular_provider::kCode, shill::kOperatorCodeKey},
    {::onc::cellular_provider::kCountry, shill::kOperatorCountryKey},
    {::onc::cellular_provider::kName, shill::kOperatorNameKey},
    {nullptr}};

const FieldTranslationEntry sim_lock_status_fields[] = {
    {::onc::sim_lock_status::kLockEnabled, shill::kSIMLockEnabledProperty},
    {::onc::sim_lock_status::kLockType, shill::kSIMLockTypeProperty},
    {::onc::sim_lock_status::kRetriesLeft, shill::kSIMLockRetriesLeftProperty},
    {nullptr}};

// This must only contain Service properties and not Device properties.
// For Device properties see kCellularDeviceTable.
const FieldTranslationEntry cellular_fields[] = {
    {::onc::cellular::kActivationType, shill::kActivationTypeProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kActivationState, shill::kActivationStateProperty},
    {::onc::cellular::kAutoConnect, shill::kAutoConnectProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kNetworkTechnology,
    //   shill::kNetworkTechnologyProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kPaymentPortal, shill::kPaymentPortal},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kRoamingState, shill::kRoamingStateProperty},
    {::onc::cellular::kSignalStrength, shill::kSignalStrengthProperty},
    {nullptr}};

const FieldTranslationEntry network_fields[] = {
    {::onc::network_config::kGUID, shill::kGuidProperty},
    {::onc::network_config::kConnectable, shill::kConnectableProperty},
    {::onc::network_config::kPriority, shill::kPriorityProperty},

    // Shill doesn't allow setting the name for non-VPN networks.
    // Name is conditionally translated, see onc_translator_*.
    // {::onc::network_config::kName, shill::kNameProperty },

    // Type is converted during translation, see onc_translator_*.
    // {::onc::network_config::kType, shill::kTypeProperty },
    // {::onc::network_config::kProxySettings, shill::ProxyConfig},

    // These fields are converted during translation, see
    // onc_translator_shill_to_onc.cc. They are only converted when going from
    // Shill->ONC, and ignored otherwise.
    // {::onc::network_config::kConnectionState, shill::kStateProperty },
    // {::onc::network_config::kErrorState, shill::kErrorProperty},
    // {::onc::network_config::kRestrictedConnectivity, shill::kStateProperty },
    // {::onc::network_config::kSource, shill::kProfileProperty },
    // {::onc::network_config::kMacAddress, shill::kAddressProperty },
    {nullptr}};

const FieldTranslationEntry ipconfig_fields[] = {
    {::onc::ipconfig::kIPAddress, shill::kAddressProperty},
    {::onc::ipconfig::kGateway, shill::kGatewayProperty},
    {::onc::ipconfig::kRoutingPrefix, shill::kPrefixlenProperty},
    {::onc::ipconfig::kNameServers, shill::kNameServersProperty},
    // This field is converted during translation, see ShillToONCTranslator::
    // TranslateIPConfig. It is only converted from Shill->ONC.
    // { ::onc::ipconfig::kType, shill::kMethodProperty},
    {::onc::ipconfig::kWebProxyAutoDiscoveryUrl,
     shill::kWebProxyAutoDiscoveryUrlProperty},
    {nullptr}};

const FieldTranslationEntry static_or_saved_ipconfig_fields[] = {
    {::onc::ipconfig::kIPAddress, shill::kAddressProperty},
    {::onc::ipconfig::kGateway, shill::kGatewayProperty},
    {::onc::ipconfig::kRoutingPrefix, shill::kPrefixlenProperty},
    {::onc::ipconfig::kNameServers, shill::kNameServersProperty},
    {::onc::ipconfig::kSearchDomains, shill::kSearchDomainsProperty},
    {::onc::ipconfig::kIncludedRoutes, shill::kIncludedRoutesProperty},
    {::onc::ipconfig::kExcludedRoutes, shill::kExcludedRoutesProperty},
    {nullptr}};

struct OncValueTranslationEntry {
  const OncValueSignature* onc_signature;
  const FieldTranslationEntry* field_translation_table;
};

const OncValueTranslationEntry onc_value_translation_table[] = {
    {&kEAPSignature, eap_fields},
    {&kIPsecSignature, ipsec_fields},
    {&kL2TPSignature, l2tp_fields},
    {&kXAUTHSignature, xauth_fields},
    {&kOpenVPNSignature, openvpn_fields},
    {&kARCVPNSignature, arc_vpn_fields},
    {&kVerifyX509Signature, verify_x509_fields},
    {&kVPNSignature, vpn_fields},
    {&kTetherSignature, tether_fields},
    {&kTetherWithStateSignature, tether_fields},
    {&kWiFiSignature, wifi_fields},
    {&kWiFiWithStateSignature, wifi_fields},
    {&kCellularApnSignature, cellular_apn_fields},
    {&kCellularFoundNetworkSignature, cellular_found_network_fields},
    {&kCellularPaymentPortalSignature, cellular_payment_portal_fields},
    {&kCellularProviderSignature, cellular_provider_fields},
    {&kSIMLockStatusSignature, sim_lock_status_fields},
    {&kCellularSignature, cellular_fields},
    {&kCellularWithStateSignature, cellular_fields},
    {&kNetworkWithStateSignature, network_fields},
    {&kNetworkConfigurationSignature, network_fields},
    {&kIPConfigSignature, ipconfig_fields},
    {&kSavedIPConfigSignature, static_or_saved_ipconfig_fields},
    {&kStaticIPConfigSignature, static_or_saved_ipconfig_fields},
    {nullptr}};

struct NestedShillDictionaryEntry {
  const OncValueSignature* onc_signature;
  // nullptr terminated list of Shill property keys.
  const char* const* shill_property_path;
};

const char* cellular_apn_path_entries[] = {shill::kCellularApnProperty,
                                           nullptr};

const char* static_ip_config_path_entries[] = {shill::kStaticIPConfigProperty,
                                               nullptr};

const NestedShillDictionaryEntry nested_shill_dictionaries[] = {
    {&kCellularApnSignature, cellular_apn_path_entries},
    {&kStaticIPConfigSignature, static_ip_config_path_entries},
    {nullptr}};

}  // namespace

const StringTranslationEntry kNetworkTypeTable[] = {
    {::onc::network_type::kEthernet, shill::kTypeEthernet},
    // kTypeEthernetEap is set in onc_translator_onc_to_shill.cc.
    //  { ::onc::network_type::kEthernet, shill::kTypeEthernetEap },
    {::onc::network_type::kWiFi, shill::kTypeWifi},
    // wimax entries are ignored in onc_translator_onc_to_shill.cc.
    // {::onc::network_type::kWimax, shill::kTypeWimax},
    {::onc::network_type::kCellular, shill::kTypeCellular},
    {::onc::network_type::kVPN, shill::kTypeVPN},
    {::onc::network_type::kTether, kTypeTether},
    {nullptr}};

const StringTranslationEntry kVPNTypeTable[] = {
    {::onc::vpn::kTypeL2TP_IPsec, shill::kProviderL2tpIpsec},
    {::onc::vpn::kOpenVPN, shill::kProviderOpenVpn},
    {::onc::vpn::kThirdPartyVpn, shill::kProviderThirdPartyVpn},
    {::onc::vpn::kArcVpn, shill::kProviderArcVpn},
    {nullptr}};

const StringTranslationEntry kWiFiSecurityTable[] = {
    {::onc::wifi::kSecurityNone, shill::kSecurityNone},
    {::onc::wifi::kWEP_PSK, shill::kSecurityWep},
    {::onc::wifi::kWPA_PSK, shill::kSecurityPsk},
    {::onc::wifi::kWPA_EAP, shill::kSecurity8021x},
    {::onc::wifi::kWEP_8021X, shill::kSecurityWep},
    {nullptr}};

const StringTranslationEntry kEAPOuterTable[] = {
    {::onc::eap::kPEAP, shill::kEapMethodPEAP},
    {::onc::eap::kEAP_TLS, shill::kEapMethodTLS},
    {::onc::eap::kEAP_TTLS, shill::kEapMethodTTLS},
    {::onc::eap::kLEAP, shill::kEapMethodLEAP},
    {nullptr}};

// Translation of the EAP.Inner field in case of EAP.Outer == PEAP
const StringTranslationEntry kEAP_PEAP_InnerTable[] = {
    {::onc::eap::kGTC, shill::kEapPhase2AuthPEAPGTC},
    {::onc::eap::kMD5, shill::kEapPhase2AuthPEAPMD5},
    {::onc::eap::kMSCHAPv2, shill::kEapPhase2AuthPEAPMSCHAPV2},
    {nullptr}};

// Translation of the EAP.Inner field in case of EAP.Outer == TTLS
const StringTranslationEntry kEAP_TTLS_InnerTable[] = {
    {::onc::eap::kGTC, shill::kEapPhase2AuthTTLSGTC},
    {::onc::eap::kMD5, shill::kEapPhase2AuthTTLSMD5},
    {::onc::eap::kMSCHAP, shill::kEapPhase2AuthTTLSMSCHAP},
    {::onc::eap::kMSCHAPv2, shill::kEapPhase2AuthTTLSMSCHAPV2},
    {::onc::eap::kPAP, shill::kEapPhase2AuthTTLSPAP},
    {nullptr}};

const StringTranslationEntry kActivationStateTable[] = {
    {::onc::cellular::kActivated, shill::kActivationStateActivated},
    {::onc::cellular::kActivating, shill::kActivationStateActivating},
    {::onc::cellular::kNotActivated, shill::kActivationStateNotActivated},
    {::onc::cellular::kPartiallyActivated,
     shill::kActivationStatePartiallyActivated},
    {nullptr}};

const StringTranslationEntry kNetworkTechnologyTable[] = {
    {::onc::cellular::kTechnologyCdma1Xrtt, shill::kNetworkTechnology1Xrtt},
    {::onc::cellular::kTechnologyGsm, shill::kNetworkTechnologyGsm},
    {::onc::cellular::kTechnologyEdge, shill::kNetworkTechnologyEdge},
    {::onc::cellular::kTechnologyEvdo, shill::kNetworkTechnologyEvdo},
    {::onc::cellular::kTechnologyGprs, shill::kNetworkTechnologyGprs},
    {::onc::cellular::kTechnologyHspa, shill::kNetworkTechnologyHspa},
    {::onc::cellular::kTechnologyHspaPlus, shill::kNetworkTechnologyHspaPlus},
    {::onc::cellular::kTechnologyLte, shill::kNetworkTechnologyLte},
    {::onc::cellular::kTechnologyLteAdvanced,
     shill::kNetworkTechnologyLteAdvanced},
    {::onc::cellular::kTechnologyUmts, shill::kNetworkTechnologyUmts},
    {nullptr}};

const StringTranslationEntry kRoamingStateTable[] = {
    {::onc::cellular::kRoamingHome, shill::kRoamingStateHome},
    {::onc::cellular::kRoamingRoaming, shill::kRoamingStateRoaming},
    {nullptr}};

const StringTranslationEntry kTetheringStateTable[] = {
    {::onc::tethering_state::kTetheringConfirmedState,
     shill::kTetheringConfirmedState},
    {::onc::tethering_state::kTetheringNotDetectedState,
     shill::kTetheringNotDetectedState},
    {::onc::tethering_state::kTetheringSuspectedState,
     shill::kTetheringSuspectedState},
    {nullptr}};

const StringTranslationEntry kOpenVpnCompressionAlgorithmTable[] = {
    {::onc::openvpn_compression_algorithm::kFramingOnly,
     shill::kOpenVPNCompressFramingOnly},
    {::onc::openvpn_compression_algorithm::kLz4, shill::kOpenVPNCompressLz4},
    {::onc::openvpn_compression_algorithm::kLz4V2,
     shill::kOpenVPNCompressLz4V2},
    {::onc::openvpn_compression_algorithm::kLzo, shill::kOpenVPNCompressLzo},
    {nullptr}};

// This must contain only Shill Device properties and no Service properties.
// For Service properties see cellular_fields.
const FieldTranslationEntry kCellularDeviceTable[] = {
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kAPNList, shill::kCellularApnListProperty},
    {::onc::cellular::kAllowRoaming, shill::kCellularAllowRoamingProperty},
    {::onc::cellular::kESN, shill::kEsnProperty},
    {::onc::cellular::kFamily, shill::kTechnologyFamilyProperty},
    {::onc::cellular::kFirmwareRevision, shill::kFirmwareRevisionProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kFoundNetworks, shill::kFoundNetworksProperty},
    {::onc::cellular::kHardwareRevision, shill::kHardwareRevisionProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kHomeProvider, shill::kHomeProviderProperty},
    {::onc::cellular::kICCID, shill::kIccidProperty},
    {::onc::cellular::kIMEI, shill::kImeiProperty},
    {::onc::cellular::kIMSI, shill::kImsiProperty},
    {::onc::cellular::kManufacturer, shill::kManufacturerProperty},
    {::onc::cellular::kMDN, shill::kMdnProperty},
    {::onc::cellular::kMEID, shill::kMeidProperty},
    {::onc::cellular::kMIN, shill::kMinProperty},
    {::onc::cellular::kModelID, shill::kModelIdProperty},
    {::onc::cellular::kScanning, shill::kScanningProperty},
    // This field is converted during translation, see onc_translator_*.
    // { ::onc::cellular::kSIMLockStatus, shill::kSIMLockStatusProperty},
    {::onc::cellular::kSIMPresent, shill::kSIMPresentProperty},
    {::onc::cellular::kSupportNetworkScan, shill::kSupportNetworkScanProperty},
    {nullptr}};

const FieldTranslationEntry* GetFieldTranslationTable(
    const OncValueSignature& onc_signature) {
  for (const OncValueTranslationEntry* it = onc_value_translation_table;
       it->onc_signature != nullptr; ++it) {
    if (it->onc_signature == &onc_signature)
      return it->field_translation_table;
  }
  return nullptr;
}

std::vector<std::string> GetPathToNestedShillDictionary(
    const OncValueSignature& onc_signature) {
  std::vector<std::string> shill_property_path;
  for (const NestedShillDictionaryEntry* it = nested_shill_dictionaries;
       it->onc_signature != nullptr; ++it) {
    if (it->onc_signature == &onc_signature) {
      for (const char* const* key = it->shill_property_path; *key != nullptr;
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
  for (const FieldTranslationEntry* it = table; it->onc_field_name != nullptr;
       ++it) {
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
  for (int i = 0; table[i].onc_value != nullptr; ++i) {
    if (onc_value != table[i].onc_value)
      continue;
    *shill_value = table[i].shill_value;
    return true;
  }
  LOG(ERROR) << "Value '" << onc_value << "' cannot be translated to Shill"
             << " table[0]: " << table[0].onc_value << " => "
             << table[0].shill_value;
  return false;
}

bool TranslateStringToONC(const StringTranslationEntry table[],
                          const std::string& shill_value,
                          std::string* onc_value) {
  for (int i = 0; table[i].shill_value != nullptr; ++i) {
    if (shill_value != table[i].shill_value)
      continue;
    *onc_value = table[i].onc_value;
    return true;
  }
  LOG(ERROR) << "Value '" << shill_value << "' cannot be translated to ONC"
             << " table[0]: " << table[0].shill_value << " => "
             << table[0].onc_value;
  return false;
}

}  // namespace onc
}  // namespace chromeos
